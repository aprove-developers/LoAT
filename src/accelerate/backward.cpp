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

using namespace std;

typedef BackwardAcceleration Self;

BackwardAcceleration::BackwardAcceleration(VarMan &varMan, const LinearRule &rule, LocationIdx sink)
        : varMan(varMan), rule(rule), sink(sink) {}

bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPolynomial();
}

vector<Rule> BackwardAcceleration::replaceByUpperbounds(const ExprSymbol &N, const Rule &rule) {
    // gather all upper bounds (if possible)
    VarEliminator ve(rule.getGuard(), N, varMan);

    // avoid rule explosion (by not instantiating N if there are too many bounds)
    if (ve.getRes().empty() || ve.getRes().size() > Config::BackwardAccel::MaxUpperboundsForPropagation) {
        return {rule};
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

LinearRule BackwardAcceleration::buildNontermRule() const {
    LinearRule res(rule.getLhsLoc(), rule.getGuard(), Expression::NontermSymbol, sink, {});
    return std::move(res);
}

Self::AccelerationResult BackwardAcceleration::run() {
    if (!shouldAccelerate()) {
        return {.res={}, .status=ForwardAcceleration::NotSupported};
    }

    option<AccelerationProblem> ap = AccelerationCalculus::init(rule, varMan);
    if (ap) {
        option<AccelerationProblem> solved = AccelerationCalculus::solve(ap.get());
        if (solved) {
            ForwardAcceleration::ResultKind status = solved->equivalent ? ForwardAcceleration::Success : ForwardAcceleration::SuccessWithRestriction;
            if (solved->nonterm) {
                return {{buildNontermRule()}, .status=status};
            } else {
                UpdateMap up;
                for (auto p: solved.get().closed) {
                    up[varMan.getVarIdx(Expression(p.first).getAVariable())] = p.second;
                }
                LinearRule res(rule.getLhsLoc(), solved.get().res, solved.get().cost, rule.getRhsLoc(), up);

                if (Config::BackwardAccel::ReplaceTempVarByUpperbounds) {
                    return {.res=replaceByUpperbounds(solved->n, res), .status=status};
                } else {
                    return {{res}, .status=status};
                }
            }
        } else {
            return {{}, ForwardAcceleration::NonMonotonic};
        }
    } else {
        return {{}, ForwardAcceleration::NoClosedFrom};
    }
}


Self::AccelerationResult BackwardAcceleration::accelerate(VarMan &varMan, const LinearRule &rule, LocationIdx sink) {
    BackwardAcceleration ba(varMan, rule, sink);
    return ba.run();
}
