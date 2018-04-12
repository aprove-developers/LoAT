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

#ifndef RULE_H
#define RULE_H

#include <map>
#include <vector>

#include "expression.h"


typedef int LocationIdx;
typedef int VariableIdx;

typedef std::vector<Expression> GuardList;
typedef std::map<VariableIdx,Expression> UpdateMap;


struct RuleLhs {
    LocationIdx loc;
    GuardList guard;
    Expression cost;

    RuleLhs() {}
    RuleLhs(LocationIdx loc, GuardList guard); // cost 1
    RuleLhs(LocationIdx loc, GuardList guard, Expression cost);
    void collectSymbols(ExprSymbolSet &set) const;
};

struct RuleRhs {
    LocationIdx loc;
    UpdateMap update;

    RuleRhs() {}
    RuleRhs(LocationIdx loc, UpdateMap update);

    // Note: only collect the update expressions, not the updated variables
    // (they belong to loc's lhs, not to this rule)
    void collectSymbols(ExprSymbolSet &set) const;
};

struct LinearRule {
    RuleLhs lhs;
    RuleRhs rhs;

    LinearRule() {};
    LinearRule(RuleLhs lhs, RuleRhs rhs);
    LinearRule(LocationIdx lhsLoc, GuardList guard, LocationIdx rhsLoc, UpdateMap update); // cost 1
    LinearRule(LocationIdx lhsLoc, GuardList guard, Expression cost, LocationIdx rhsLoc, UpdateMap update);
    void collectSymbols(ExprSymbolSet &set) const;
};

struct NonlinearRule {
    RuleLhs lhs;
    std::vector<RuleRhs> rhss;

    NonlinearRule() {}
    NonlinearRule(RuleLhs lhs, std::vector<RuleRhs> rhss);

    bool isLinear() const;
    LinearRule toLinearRule() const;
    void collectSymbols(ExprSymbolSet &set) const;
};


#endif // RULE_H
