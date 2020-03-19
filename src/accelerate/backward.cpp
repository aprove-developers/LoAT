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

#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"

#include "recurrence/recurrence.hpp"
#include "meter/metertools.hpp"
#include "../expr/guardtoolbox.hpp"
#include "forward.hpp"

#include <purrs.hh>
#include "../util/relevantvariables.hpp"
#include "../analysis/chain.hpp"
#include "accelerationproblem.hpp"
#include "vareliminator.hpp"
#include "../util/result.hpp"
#include "../its/export.hpp"

using namespace std;

typedef BackwardAcceleration Self;

BackwardAcceleration::BackwardAcceleration(ITSProblem &its, const LinearRule &rule, LocationIdx sink)
        : its(its), rule(rule), sink(sink) {}

bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPoly();
}

vector<Rule> BackwardAcceleration::replaceByUpperbounds(const Var &N, const Rule &rule) {
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
        for (const Subs &subs : ve.getRes()) {
            res.push_back(rule.subs(subs));
        }
    }
    return res;
}

LinearRule BackwardAcceleration::buildNontermRule(const GuardList &guard) const {
    return LinearRule(rule.getLhsLoc(), guard, Expr::NontermSymbol, sink, {});
}

Self::AccelerationResult BackwardAcceleration::run() {
    AccelerationResult res;
    res.status = Failure;
    if (shouldAccelerate()) {
        option<AccelerationProblem> ap = AccelerationCalculus::init(rule, its);
        if (ap) {
            ap->simplifyEquivalently();
            if (ap->solved()) {
                res.status = Success;
                if (ap->nonterm) {
                    const Rule &nontermRule = buildNontermRule(ap->res);
                    res.rules.push_back(nontermRule);
                    res.proof.ruleTransformationProof(rule, "nonterm", nontermRule, its);
                    res.proof.storeSubProof(ap->proof, "acceration calculus");
                } else {
                    Subs up;
                    for (auto p: ap->closed) {
                        up.put(p.first, p.second);
                    }
                    LinearRule accel(rule.getLhsLoc(), ap->res, ap->cost, rule.getRhsLoc(), up);
                    res.proof.ruleTransformationProof(rule, "acceleration", accel, its);
                    res.proof.storeSubProof(ap->proof, "acceration calculus");
                    std::vector<Rule> instantiated = replaceByUpperbounds(ap->n, accel);
                    if (instantiated.empty()) {
                        res.rules.push_back(accel);
                    } else {
                        for (const Rule &r: instantiated) {
                            res.proof.ruleTransformationProof(accel, "instantiation", r, its);
                            res.rules.push_back(r);
                        }
                    }
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
