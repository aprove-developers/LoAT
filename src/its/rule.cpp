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


RuleRhs::RuleRhs(LocationIdx loc, UpdateMap update) : loc(loc), update(update)
{}


LinearRule::LinearRule(RuleLhs lhs, RuleRhs rhs) : lhs(lhs), rhs(rhs)
{}

LinearRule::LinearRule(LocationIdx lhsLoc, GuardList guard, LocationIdx rhsLoc, UpdateMap update)
        : lhs(lhsLoc, guard), rhs(rhsLoc, update)
{}

LinearRule::LinearRule(LocationIdx lhsLoc, GuardList guard, Expression cost, LocationIdx rhsLoc, UpdateMap update)
        : lhs(lhsLoc, guard, cost), rhs(rhsLoc, update)
{}

LinearRule LinearRule::withNewRhsLoc(LocationIdx rhsLoc) const {
    return LinearRule(lhs, RuleRhs(rhsLoc, rhs.update));
}



NonlinearRule::NonlinearRule(RuleLhs lhs, std::vector<RuleRhs> rhss) : lhs(lhs), rhss(rhss) {
    assert(!rhss.empty());
}

bool NonlinearRule::isLinear() const {
    return rhss.size() == 1;
}

LinearRule NonlinearRule::toLinearRule() const {
    assert(isLinear());
    return LinearRule(lhs, rhss.front());
}


std::ostream& operator<<(std::ostream &s, const LinearRule &rule) {
    s << "LRule(";
    s << rule.getLhsLoc() << " | ";

    for (auto upit : rule.getUpdate()) {
        s << upit.first << "=" << upit.second;
        s << ", ";
    }

    s << "| ";

    for (auto expr : rule.getGuard()) {
        s << expr << ", ";
    }
    s << "| ";
    s << rule.getCost();
    s << ")";
    return s;
    }
}

