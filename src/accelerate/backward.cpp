/*  This file is part of LoAT.
 *  Copyright (c) 2018-2019 Florian Frohn
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#include "backward.hpp"

#include "../z3/z3solver.hpp"
#include "../z3/z3toolbox.hpp"

#include "recurrence/recurrence.hpp"
#include "meter/metertools.hpp"
#include "../expr/guardtoolbox.hpp"
#include "../expr/relation.hpp"
#include "../expr/ginactoz3.hpp"
#include "forward.hpp"

#include <purrs.hh>
#include "../util/relevantvariables.hpp"
#include "../analysis/chain.hpp"
#include "accelerationproblem.hpp"
#include "vareliminator.hpp"
#include "../util/result.hpp"
#include "../its/export.hpp"

#include <boost/filesystem.hpp>

using namespace std;

typedef BackwardAcceleration Self;

BackwardAcceleration::BackwardAcceleration(ITSProblem &its, const LinearRule &rule, LocationIdx sink)
        : its(its), rule(rule), sink(sink) {}

bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPolynomial();
}

vector<Rule> BackwardAcceleration::replaceByUpperbounds(const ExprSymbol &N, const Rule &rule) {
    // gather all upper bounds (if possible)
    VarEliminator ve(rule.getGuard(), N, its);

    // avoid rule explosion (by not instantiating N if there are too many bounds)
    if (ve.getRes().empty() || ve.getRes().size() > Config::BackwardAccel::MaxUpperboundsForPropagation) {
        return {};
    }

    // create one rule for each upper bound, by instantiating N with this bound
    vector<Rule> res;
    if (ve.getRes().empty()) {
        res.push_back(rule);
    } else {
        for (const GiNaC::exmap &subs : ve.getRes()) {
            Rule instantiated = rule;
            instantiated.applySubstitution(subs);
            res.push_back(instantiated);
        }
    }
    return res;
}

LinearRule BackwardAcceleration::buildNontermRule(const GuardList &guard) const {
    return LinearRule(rule.getLhsLoc(), guard, Expression::NontermSymbol, sink, {});
}

Self::AccelerationResult BackwardAcceleration::run() {
    AccelerationResult res;
    res.status = Failure;
    if (shouldAccelerate()) {
        AccelerationProblem ap = AccelerationCalculus::init(rule, its);
        option<AccelerationProblem> solved = AccelerationCalculus::solveEquivalently(ap);
        if (solved) {
            res.status = Success;
            if (solved->nonterm) {
                const Rule &nontermRule = buildNontermRule(solved->res);
                res.rules.push_back(nontermRule);
                res.proof.ruleTransformationProof(rule, "nonterm", nontermRule, its);
                res.proof.storeSubProof(solved->proof, "acceration calculus");
            } else {
                UpdateMap up;
                for (auto p: solved.get().closed.get()) {
                    up[its.getVarIdx(Expression(p.first).getAVariable())] = p.second;
                }
                LinearRule accel(rule.getLhsLoc(), solved.get().res, solved.get().cost, rule.getRhsLoc(), up);
                res.proof.ruleTransformationProof(rule, "acceleration", accel, its);
                res.proof.storeSubProof(solved->proof, "acceration calculus");
                if (Config::BackwardAccel::ReplaceTempVarByUpperbounds) {
                    std::vector<Rule> instantiated = replaceByUpperbounds(solved->n, accel);
                    if (instantiated.empty()) {
                        res.rules.push_back(accel);
                    } else {
                        for (const Rule &r: instantiated) {
                            res.proof.ruleTransformationProof(accel, "instantiation", r, its);
                            res.rules.push_back(r);
                        }
                    }
                } else {
                    res.rules.push_back(accel);
                }
            }
        }
    }
    return res;
}


Self::AccelerationResult BackwardAcceleration::accelerate(ITSProblem &its, const LinearRule &rule, LocationIdx sink) {
    BackwardAcceleration ba(its, rule, sink);
    return ba.run();
}
