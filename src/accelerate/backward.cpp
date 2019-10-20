
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
    for (const Expression &ex: rule.getGuard()) {
        if (Relation::isEquality(ex)) {
            guard.push_back(ex.lhs() - ex.rhs() + 1 > 0);
            guard.push_back(ex.rhs() - ex.lhs() + 1 > 0);
        } else {
            guard.push_back(Relation::normalizeInequality(ex));
        }
    }
}

bool BackwardAcceleration::computeInvarianceSplit() {
    // find conditional invariants (temporarily stored in simpleInvariants), store all other constraints in 'decreasing'
    decreasing = MeteringToolbox::reduceGuard(varMan, guard, {update}, &simpleInvariants);
    // check if simpleInvariants is a simple invariant
    bool done;
    do {
        done = true;
        auto it = simpleInvariants.begin();
        while (it != simpleInvariants.end()) {
            GuardList updated;
            updated.push_back((*it).subs(updateSubs));
            if (Z3Toolbox::isValidImplication(simpleInvariants, updated)) {
                it++;
                continue;
            }
            // no simple invariant -- move the problematic constraint to conditionalInvariants and retry
            conditionalInvariants.push_back(*it);
            simpleInvariants.erase(it);
            done = false;
            break;
        }
    } while (!done);
    // from now on, we may assume that simpleInvariants always holds
    Z3Context ctx;
    Z3Solver solver(ctx);
    for (const Expression &ex: simpleInvariants) {
        solver.add(ex.toZ3(ctx));
    }
    auto it = decreasing.begin();
    while (it != decreasing.end()) {
        solver.push();
        solver.add(GinacToZ3::convert(it->subs(updateSubs), ctx));
        if (solver.check() == z3::check_result::sat) {
            solver.add(GinacToZ3::convert(it->lhs() <= 0, ctx));
            if (solver.check() == z3::check_result::unsat) {
                it++;
                solver.pop();
                continue;
            }
        }
        return false;
    }
    return true;
}

bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPolynomial();
}

LinearRule BackwardAcceleration::buildAcceleratedLoop(const UpdateMap &iteratedUpdate,
                                                const Expression &iteratedCost,
                                                const GuardList &restrictions,
                                                const ExprSymbol &N,
                                                const unsigned int validityBound) const
{
    assert(validityBound <= 1);
    GiNaC::exmap updateSubs = iteratedUpdate.toSubstitution(varMan);
    GuardList newGuard = restrictions;
    newGuard.push_back(N >= validityBound);
    for (const Expression &ex : simpleInvariants) {
        newGuard.push_back(ex);
    }
    for (const Expression &ex : conditionalInvariants) {
        newGuard.push_back(ex);
    }
    for (const Expression &ex : decreasing) {
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

    bool applicable = computeInvarianceSplit();
    if (!applicable) {
        debugBackwardAccel("Failed to check guard implication");
        Stats::add(Stats::BackwardNonMonotonic);
        return {{}, ForwardAcceleration::NonMonotonic};
    }

    // compute the iterated update and cost, with a fresh variable N as iteration step
    ExprSymbol N = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("n"));

    option<LinearRule> accelerated;
    GuardList restrictions;
    option<Recurrence::IteratedUpdates> recRes = Recurrence::iterateUpdates(varMan, {rule.getUpdate()}, N);
    if (!recRes || recRes.get().validityBound > 1) {
        debugBackwardAccel("Failed to compute iterated cost/update");
        Stats::add(Stats::BackwardCannotIterate);
        return {{}, ForwardAcceleration::NoClosedFrom};
    }

    // compute the resulting rule and try to simplify it by instantiating N (if enabled)
    accelerated = buildAcceleratedLoop(recRes.get().updates[0], Expression(1), restrictions, N, recRes.get().validityBound);
    Stats::add(Stats::BackwardSuccess);
    return {{accelerated.get()}, ForwardAcceleration::Success};
}


Self::AccelerationResult BackwardAcceleration::accelerate(VarMan &varMan, const LinearRule &rule, const LocationIdx &sink) {
    BackwardAcceleration ba(varMan, rule, sink);
    return ba.run();
}
