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
#include "accelerationproblem.hpp"

using namespace std;

typedef BackwardAcceleration Self;

BackwardAcceleration::BackwardAcceleration(VarMan &varMan, const LinearRule &rule)
        : varMan(varMan), rule(rule) {}

bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPolynomial();
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


vector<Rule> BackwardAcceleration::replaceByUpperbounds(const ExprSymbol &N, const Rule &rule) {
    // gather all upper bounds (if possible)
    auto bounds = computeUpperbounds(N, rule.getGuard());

    // avoid rule explosion (by not instantiating N if there are too many bounds)
    if (bounds.empty() || bounds.size() > Config::BackwardAccel::MaxUpperboundsForPropagation) {
        return {rule};
    }

    // create one rule for each upper bound, by instantiating N with this bound
    vector<Rule> res;
    for (const Expression &bound : bounds) {
        GiNaC::exmap subs;
        subs[N] = bound;

        Rule instantiated = rule;
        instantiated.applySubstitution(subs);
        res.push_back(instantiated);
        debugBackwardAccel("instantiation " << subs << " yielded " << instantiated);
    }
    return res;
}

Self::AccelerationResult BackwardAcceleration::run() {
    if (!shouldAccelerate()) {
        debugBackwardAccel("won't try to accelerate transition with costs " << rule.getCost());
        return {.res={}, .status=ForwardAcceleration::NotSupported};
    }
    debugBackwardAccel("Trying to accelerate rule " << rule);

    option<AccelerationProblem> ap = AccelerationCalculus::init(rule, varMan);
    if (ap) {
        option<AccelerationProblem> solved = AccelerationCalculus::solve(ap.get());
        if (solved) {
            UpdateMap up;
            for (auto p: solved.get().closed) {
                up[varMan.getVarIdx(Expression(p.first).getAVariable())] = p.second;
            }
            LinearRule res(rule.getLhsLoc(), solved.get().res, solved.get().cost, rule.getRhsLoc(), up);
            Stats::add(Stats::BackwardSuccess);
            if (Config::BackwardAccel::ReplaceTempVarByUpperbounds) {
                return {.res=replaceByUpperbounds(solved->n, res), .status=ForwardAcceleration::Success};
            } else {
                return {{res}, .status=ForwardAcceleration::Success};
            }
        } else {
            return {{}, ForwardAcceleration::NonMonotonic};
        }
    } else {
        return {{}, ForwardAcceleration::NoClosedFrom};
    }
}


Self::AccelerationResult BackwardAcceleration::accelerate(VarMan &varMan, const LinearRule &rule) {
    BackwardAcceleration ba(varMan, rule);
    return ba.run();
}
