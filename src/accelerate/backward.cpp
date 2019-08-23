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

#include "../debug.hpp"
#include "../z3/z3solver.hpp"
#include "../z3/z3toolbox.hpp"

#include "recurrence/recurrence.hpp"
#include "meter/metertools.hpp"
#include "../expr/guardtoolbox.hpp"
#include "../expr/relation.hpp"
#include "../expr/ginactoz3.hpp"
#include "forward.hpp"

#include <purrs.hh>
#include "../util/stats.hpp"
#include "../util/relevantvariables.hpp"
#include "../analysis/chain.hpp"

using namespace std;

typedef BackwardAcceleration Self;

BackwardAcceleration::BackwardAcceleration(VarMan &varMan, const LinearRule &rule, const LocationIdx &sink)
        : varMan(varMan), rule(rule), sink(sink), update(rule.getUpdate()), updateSubs(rule.getUpdate().toSubstitution(varMan)) {
    computeInvarianceSplit();
}

void BackwardAcceleration::computeInvarianceSplit() {
    nonInvariants = MeteringToolbox::reduceGuard(varMan, rule.getGuard(), {update}, &simpleInvariants);
    bool done;
    do {
        done = true;
        auto it = simpleInvariants.begin();
        while (it != simpleInvariants.end()) {
            GuardList updated;
            updated.push_back((*it).subs(updateSubs));
            if (Z3Toolbox::isValidImplication(simpleInvariants, updated)) {
                it++;
            } else {
                simpleInvariants.erase(it);
                conditionalInvariants.push_back(*it);
                done = false;
                break;
            }
        }
    } while (!done);
}

bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPolynomial();
}

bool BackwardAcceleration::checkGuardImplication() const {
    Z3Context context;
    Z3Solver solver(context);
    z3::expr_vector rhss(context);

    for (const Expression &ex : simpleInvariants) {
        solver.add(GinacToZ3::convert(ex, context));
    }

    for (const Expression &ex : nonInvariants) {
        solver.add(GinacToZ3::convert(ex.subs(updateSubs), context));
        rhss.push_back(GinacToZ3::convert(ex, context));
    }
    if (solver.check() != z3::sat) {
        return false;
    }
    solver.add(!z3::mk_and(rhss));
    if (solver.check() != z3::unsat) {
        return false;
    }
    return true;
}

LinearRule BackwardAcceleration::buildNontermRule() const {
    LinearRule res(rule.getLhsLoc(), rule.getGuard(), Expression::NontermSymbol, sink, {});
    debugBackwardAccel("backward-accelerating " << rule << " yielded " << res);
    return std::move(res);
}

LinearRule BackwardAcceleration::buildAcceleratedLoop(const UpdateMap &iteratedUpdate,
                                                const Expression &iteratedCost,
                                                const GuardList &strengthenedGuard,
                                                const ExprSymbol &N,
                                                const unsigned int validityBound) const
{
    assert(validityBound <= 1);
    GiNaC::exmap updateSubs = iteratedUpdate.toSubstitution(varMan);
    GuardList newGuard = strengthenedGuard;
    newGuard.push_back(N >= validityBound);
    for (const Expression &ex : nonInvariants) {
        newGuard.erase(std::find(newGuard.begin(), newGuard.end(), ex));
        newGuard.push_back(ex.subs(updateSubs).subs(N == N-1)); // apply the update N-1 times
    }
    LinearRule res(rule.getLhsLoc(), newGuard, iteratedCost, rule.getRhsLoc(), iteratedUpdate);
    debugBackwardAccel("backward-accelerating " << rule << " yielded " << res);
    return std::move(res);
}

vector<Expression> BackwardAcceleration::computeUpperbounds(const ExprSymbol &N, const GuardList &guard) {
    // First check if there is an equality constraint (we can then ignore all other upper bounds)
    for (const Expression &ex : guard) {
        if (Relation::isEquality(ex) && ex.has(N)) {
            auto optSolved = GuardToolbox::solveTermFor(ex.lhs()-ex.rhs(), N, GuardToolbox::ResultMapsToInt);
            if (!optSolved) {
                debugBackwardAccel("unable to compute upperbound from equality " << ex);
                return {};
            }
            // One equality is enough, as all other bounds must also satisfy this equality
            return vector<Expression>({optSolved.get()});
        }
    }

    // Otherwise, collect all upper bounds
    vector<Expression> bounds;
    for (const Expression &ex : guard) {
        if (Relation::isEquality(ex) || !ex.has(N)) continue;

        Expression term = Relation::toLessEq(ex);
        term = (term.lhs() - term.rhs()).expand();
        if (term.degree(N) != 1) continue;

        // ignore lower bounds (terms of the form -N <= 0)
        if (term.coeff(N, 1).info(GiNaC::info_flags::negative)) {
            continue;
        }

        // compute the upper bound represented by N and check that it is integral
        auto optSolved = GuardToolbox::solveTermFor(term, N, GuardToolbox::ResultMapsToInt);
        if (!optSolved) {
            debugBackwardAccel("unable to compute upperbound from " << ex);
            return {};
        }
        bounds.push_back(optSolved.get());
    }

    if (bounds.empty()) {
        debugBackwardAccel("warning: no upperbounds found, not instantiating " << N);
        return {};
    }

    return bounds;
}


vector<LinearRule> BackwardAcceleration::replaceByUpperbounds(const ExprSymbol &N, const LinearRule &rule) {
    // gather all upper bounds (if possible)
    auto bounds = computeUpperbounds(N, rule.getGuard());

    // avoid rule explosion (by not instantiating N if there are too many bounds)
    if (bounds.empty() || bounds.size() > Config::BackwardAccel::MaxUpperboundsForPropagation) {
        return {rule};
    }

    // create one rule for each upper bound, by instantiating N with this bound
    vector<LinearRule> res;
    for (const Expression &bound : bounds) {
        GiNaC::exmap subs;
        subs[N] = bound;

        LinearRule instantiated = rule;
        instantiated.applySubstitution(subs);
        res.push_back(instantiated);
        debugBackwardAccel("instantiation " << subs << " yielded " << instantiated);
    }
    return res;
}

Self::AccelerationResult BackwardAcceleration::run() {
    if (!shouldAccelerate()) {
        debugBackwardAccel("won't try to accelerate transition with costs " << rule.getCost());
        return {{}, ForwardAcceleration::NotSupported};
    }
    debugBackwardAccel("Trying to accelerate rule " << rule);

    if (nonInvariants.empty() && Z3Toolbox::isValidImplication(rule.getGuard(), {rule.getCost() > 0})) {
        return {{buildNontermRule()}, ForwardAcceleration::Success};
    }

    if (!checkGuardImplication()) {
        debugBackwardAccel("Failed to check guard implication");
        Stats::add(Stats::BackwardNonMonotonic);
        return {{}, ForwardAcceleration::NonMonotonic};
    }

    // compute the iterated update and cost, with a fresh variable N as iteration step
    ExprSymbol N = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("k"));

    option<LinearRule> accelerated;
    option<unsigned int> validityBound;
    UpdateMap iteratedUpdate = rule.getUpdate();
    Expression iteratedCost = rule.getCost();
    GuardList strengthenedGuard = rule.getGuard();
    validityBound = Recurrence::iterateUpdateAndCost(varMan, iteratedUpdate, iteratedCost, strengthenedGuard, N);
    if (!validityBound) {
        debugBackwardAccel("Failed to compute iterated cost/update");
        Stats::add(Stats::BackwardCannotIterate);
        return {{}, ForwardAcceleration::NoClosedFrom};
    }

    // compute the resulting rule and try to simplify it by instantiating N (if enabled)
    accelerated = buildAcceleratedLoop(iteratedUpdate, iteratedCost, strengthenedGuard, N, validityBound.get());
    Stats::add(Stats::BackwardSuccess);
    if (Config::BackwardAccel::ReplaceTempVarByUpperbounds) {
        return {replaceByUpperbounds(N, accelerated.get()), ForwardAcceleration::Success};
    } else {
        return {{accelerated.get()}, ForwardAcceleration::Success};
    }
}


Self::AccelerationResult BackwardAcceleration::accelerate(VarMan &varMan, const LinearRule &rule, const LocationIdx &sink) {
    BackwardAcceleration ba(varMan, rule, sink);
    return ba.run();
}
