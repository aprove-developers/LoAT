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
    return LinearRule(lhs, RuleRhs(rhsLoc, rhs.getUpdate()));
}

bool LinearRule::isDummyRule() const {
    return lhs.guard.empty() && rhs.getUpdate().empty() && lhs.cost.is_zero();
}

LinearRule LinearRule::dummyRule(LocationIdx lhsLoc, LocationIdx rhsLoc) {
    return LinearRule(lhsLoc, {}, Expression(0), rhsLoc, {});
}

void LinearRule::applyTempVarSubstitution(const GiNaC::exmap &subs) {
    this->getCostMut().applySubs(subs);
    for (Expression &ex : this->getGuardMut()) {
        ex.applySubs(subs);
    }
    for (auto &it : this->getUpdateMut()) {
        it.second.applySubs(subs);
    }
}


NonlinearRule::NonlinearRule(RuleLhs lhs, std::vector<RuleRhs> rhss) : lhs(lhs), rhss(rhss) {
    assert(!rhss.empty());
}

NonlinearRule::NonlinearRule(LocationIdx lhsLoc, GuardList guard, Expression cost, LocationIdx rhsLoc, UpdateMap update)
        : lhs(lhsLoc, guard, cost), rhss({RuleRhs(rhsLoc, update)})
{}

NonlinearRule::NonlinearRule(RuleLhs lhs, RuleRhs rhs)
        : lhs(lhs), rhss({rhs})
{}

NonlinearRule NonlinearRule::fromLinear(const LinearRule &linRule) {
    RuleLhs lhs(linRule.getLhsLoc(), linRule.getGuard(), linRule.getCost());
    RuleRhs rhs(linRule.getRhsLoc(), linRule.getUpdate());
    return NonlinearRule(lhs, rhs);
}

NonlinearRule NonlinearRule::dummyRule(LocationIdx lhsLoc, LocationIdx rhsLoc) {
    return NonlinearRule(lhsLoc, {}, Expression(0), rhsLoc, {});
}

bool NonlinearRule::isLinear() const {
    return rhss.size() == 1;
}

LinearRule NonlinearRule::toLinearRule() const {
    assert(isLinear());
    return LinearRule(lhs, rhss.front());
}

bool NonlinearRule::isSimpleLoop() const {
    return std::all_of(rhss.begin(), rhss.end(), [&](const RuleRhs &rhs){ return rhs.getLoc() == lhs.loc; });
}

bool NonlinearRule::hasSelfLoop() const {
    return std::any_of(rhss.begin(), rhss.end(), [&](const RuleRhs &rhs){ return rhs.getLoc() == lhs.loc; });
}


void LinearRule::dumpToStream(std::ostream &s) const {
    s << "LRule(";

    // lhs (loc, guard, cost)
    s << getLhsLoc() << " | ";

    for (auto expr : getGuard()) {
        s << expr << ", ";
    }
    s << "| ";
    s << getCost();

    // rhs (loc, update)
    s << " || " << getRhsLoc() << " | ";

    for (auto upit : getUpdate()) {
        s << upit.first << "=" << upit.second;
        s << ", ";
    }

    s << ")";
}

void NonlinearRule::dumpToStream(std::ostream &s) const {
    s << "NLRule(";

    // lhs (loc, guard, cost)
    s << getLhsLoc() << " | ";

    for (auto expr : getGuard()) {
        s << expr << ", ";
    }
    s << "| ";
    s << getCost();

    // rhs (loc, update)*
    s << " |";

    for (auto rhs = rhsBegin(); rhs != rhsEnd(); ++rhs) {
        s << "| " << rhs->getLoc() << " | ";

        for (auto upit : rhs->getUpdate()) {
            s << upit.first << "=" << upit.second;
            s << ", ";
        }
    }

    s << ")";
}
