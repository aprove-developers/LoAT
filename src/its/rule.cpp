/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
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

#include "rule.h"

using namespace std;


RuleLhs::RuleLhs(LocationIdx loc, GuardList guard) : loc(loc), guard(guard), cost(1)
{}

RuleLhs::RuleLhs(LocationIdx loc, GuardList guard, Expression cost) : loc(loc), guard(guard), cost(cost)
{}

void RuleLhs::collectSymbols(ExprSymbolSet &set) const {
    cost.collectVariables(set);
    for (const Expression &ex : guard) {
        ex.collectVariables(set);
    }
}


RuleRhs::RuleRhs(LocationIdx loc, UpdateMap update) : loc(loc), update(update)
{}

void RuleRhs::collectSymbols(ExprSymbolSet &set) const {
    for (const auto &it : update) {
        it.second.collectVariables(set);
    }
}


LinearRule::LinearRule(RuleLhs lhs, RuleRhs rhs) : lhs(lhs), rhs(rhs)
{}

LinearRule::LinearRule(LocationIdx lhsLoc, GuardList guard, LocationIdx rhsLoc, UpdateMap update)
        : lhs(lhsLoc, guard), rhs(rhsLoc, update)
{}

LinearRule::LinearRule(LocationIdx lhsLoc, GuardList guard, Expression cost, LocationIdx rhsLoc, UpdateMap update)
        : lhs(lhsLoc, guard, cost), rhs(rhsLoc, update)
{}

void LinearRule::collectSymbols(ExprSymbolSet &set) const {
    lhs.collectSymbols(set);
    rhs.collectSymbols(set);
}


NonlinearRule::NonlinearRule(RuleLhs lhs, std::vector<RuleRhs> rhss) : lhs(lhs), rhss(rhss) {
    assert(!rhss.empty());
}

void NonlinearRule::collectSymbols(ExprSymbolSet &set) const {
    lhs.collectSymbols(set);
    for (const RuleRhs &rhs : rhss) {
        rhs.collectSymbols(set);
    }
}

bool NonlinearRule::isLinear() const {
    return rhss.size() == 1;
}

LinearRule NonlinearRule::toLinearRule() const {
    assert(isLinear());
    return LinearRule(lhs, rhss.front());
}

