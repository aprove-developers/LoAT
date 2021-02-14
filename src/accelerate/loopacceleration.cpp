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

#include "loopacceleration.hpp"

#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"
#include "../asymptotic/asymptoticbound.hpp"
#include "recurrence/recurrence.hpp"
#include "meter/metertools.hpp"
#include "../expr/guardtoolbox.hpp"

#include <purrs.hh>
#include "../util/relevantvariables.hpp"
#include "../analysis/chain.hpp"
#include "accelerationCalculus/accelerationproblem.hpp"
#include "iterationCounterElimination/vareliminator.hpp"
#include "../util/status.hpp"
#include "../its/export.hpp"

using namespace std;

LoopAcceleration::LoopAcceleration(ITSProblem &its, const LinearRule &rule, LocationIdx sink, Complexity cpx)
        : its(its), rule(rule), sink(sink), cpx(cpx) {}

bool LoopAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPoly();
}

vector<Rule> LoopAcceleration::replaceByUpperbounds(const Var &N, const Rule &rule) {
    // gather all upper bounds (if possible)
    VarEliminator ve(rule.getGuard(), N, its);

    // avoid rule explosion (by not instantiating N if there are too many bounds)
    if (ve.getRes().empty() || ve.getRes().size() > Config::LoopAccel::MaxUpperboundsForPropagation) {
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

LinearRule LoopAcceleration::buildNontermRule(const BoolExpr guard) const {
    return LinearRule(rule.getLhsLoc(), guard, Expr::NontermSymbol, sink, {});
}

Acceleration::Result LoopAcceleration::run() {
    Acceleration::Result res;
    res.status = Failure;
    if (shouldAccelerate()) {
        option<AccelerationProblem> ap = AccelerationProblem::init(rule, its);
        if (ap) {
            std::vector<AccelerationProblem::Result> ars = ap->computeRes();
            for (const AccelerationProblem::Result &ar: ars) {
                res.status = ap->getValidityBound() > 1 ? PartialSuccess : Success;
                if (ar.witnessesNonterm) {
                    const Rule &nontermRule = buildNontermRule(ar.newGuard);
                    res.rules.emplace_back(nontermRule, Complexity::Nonterm);
                    res.proof.ruleTransformationProof(rule, "nonterm", nontermRule, its);
                    res.proof.storeSubProof(ap->getProof(), "acceration calculus");
                } else {
                    LinearRule accel(rule.getLhsLoc(), ar.newGuard, ap->getAcceleratedCost(), rule.getRhsLoc(), ap->getClosedForm());
                    Complexity newCpx = AsymptoticBound::determineComplexityViaSMT(
                                its,
                                accel.getGuard(),
                                accel.getCost()).cpx;
                    if (newCpx > cpx) {
                        res.proof.ruleTransformationProof(rule, "acceleration", accel, its);
                        res.proof.storeSubProof(ap->getProof(), "acceration calculus");
                        std::vector<Rule> instantiated = replaceByUpperbounds(ap->getIterationCounter(), accel);
                        if (instantiated.empty()) {
                            res.rules.emplace_back(accel, newCpx);
                        } else {
                            for (const Rule &r: instantiated) {
                                res.proof.ruleTransformationProof(accel, "instantiation", r, its);
                                res.rules.emplace_back(r, newCpx);
                            }
                        }
                    }
                }
            }
        }
    }
    return res;
}


Acceleration::Result LoopAcceleration::accelerate(ITSProblem &its, const LinearRule &rule, LocationIdx sink, Complexity cpx) {
    LoopAcceleration ba(its, rule, sink, cpx);
    return ba.run();
}
