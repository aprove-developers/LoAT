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

#include "expr/expression.h"


typedef int LocationIdx;
typedef int VariableIdx;

typedef std::vector<Expression> GuardList;
typedef std::map<VariableIdx,Expression> UpdateMap;


// TODO: Create abstract base class Rule?
// TODO: LinearRule can implement all methods of NonlinearRule, with only minor performance impact.



struct RuleLhs {
    LocationIdx loc;
    GuardList guard;
    Expression cost;

    RuleLhs() {}
    RuleLhs(LocationIdx loc, GuardList guard); // cost 1
    RuleLhs(LocationIdx loc, GuardList guard, Expression cost);
};

struct RuleRhs {
    LocationIdx loc;
    UpdateMap update;

    RuleRhs() {}
    RuleRhs(LocationIdx loc, UpdateMap update);
};


// TODO: Make lhs, rhs private and add getLhs() or only getLhsLoc(), getGuard(), getCost(), ...
// TODO: NonlinearRule might need getUpdate(idx), getRhsLoc(idx) or getRhss()

/**
 * Interface for a rule, generalizes linear and nonlinear rules to a common interface.
 * TODO: Document how this is used (not yet sure...)
 */
class AbstractRule {
public:
    // lhs
    virtual LocationIdx getLhsLoc() const = 0;
    virtual const GuardList& getGuard() const = 0;
    virtual const Expression& getCost() const = 0;

    // rhs
    virtual const RuleRhs* rhsBegin() const = 0;
    virtual const RuleRhs* rhsEnd() const = 0;
    virtual size_t rhsCount() const = 0;
};


/**
 * A class to represent a linear rule (one lhs, one rhs).
 * The lhs/rhs locations cannot be changed (since they are tied to the graph in ITSProblem).
 * Guard/cost/update may be modified.
 */
class LinearRule : public AbstractRule {
private:
    RuleLhs lhs;
    RuleRhs rhs;

public:
    LinearRule() {};
    LinearRule(RuleLhs lhs, RuleRhs rhs);
    LinearRule(LocationIdx lhsLoc, GuardList guard, LocationIdx rhsLoc, UpdateMap update); // cost 1
    LinearRule(LocationIdx lhsLoc, GuardList guard, Expression cost, LocationIdx rhsLoc, UpdateMap update);

    // query lhs data
    LocationIdx getLhsLoc() const override { return lhs.loc; }
    const GuardList& getGuard() const override { return lhs.guard; }
    const Expression& getCost() const override { return lhs.cost; }

    // lhs mutation
    GuardList& getGuardMut() { return lhs.guard; }
    Expression& getCostMut() { return lhs.cost; }

    // iteration over right-hand sides
    const RuleRhs* rhsBegin() const override { return &rhs; }
    const RuleRhs* rhsEnd() const override { return &rhs + 1; }
    size_t rhsCount() const override { return 1; }

    // special methods for linear rules
    LocationIdx getRhsLoc() const { return rhs.loc; }
    const UpdateMap& getUpdate() const { return rhs.update; }
    UpdateMap& getUpdateMut() { return rhs.update; }
};


class NonlinearRule : public AbstractRule {
private:
    RuleLhs lhs;
    std::vector<RuleRhs> rhss;

public:
    NonlinearRule() {}
    NonlinearRule(RuleLhs lhs, std::vector<RuleRhs> rhss);

    // query lhs data
    LocationIdx getLhsLoc() const override { return lhs.loc; }
    const GuardList& getGuard() const override { return lhs.guard; }
    const Expression& getCost() const override { return lhs.cost; }

    // lhs mutation
    GuardList& getGuardMut() { return lhs.guard; }
    Expression& getCostMut() { return lhs.cost; }

    // iteration over right-hand sides
    const RuleRhs* rhsBegin() const override { return &rhss.front(); }
    const RuleRhs* rhsEnd() const override { return &rhss.back()+1; }
    size_t rhsCount() const override { return rhss.size(); }

    // special methods for nonlinear rules (idx is an index to rhss)
    LocationIdx getRhsLoc(int idx) const { return rhss[idx].loc; }
    const UpdateMap& getUpdate(int idx) const { return rhss[idx].update; };
    UpdateMap& getUpdateMut(int idx) { return rhss[idx].update; }

    // conversion to linear rule
    bool isLinear() const;
    LinearRule toLinearRule() const;
};



#endif // RULE_H
