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

#include "types.h"
#include "util/option.h"



// TODO: turn into a class
class RuleLhs {
    LocationIdx loc;
    GuardList guard;
    Expression cost;

public:
    RuleLhs(LocationIdx loc, GuardList guard) : RuleLhs(loc, guard, Expression(1)) {}
    RuleLhs(LocationIdx loc, GuardList guard, Expression cost) : loc(loc), guard(guard), cost(cost) {}

    LocationIdx getLoc() const { return loc; }
    const GuardList& getGuard() const { return guard; }
    GuardList& getGuardMut() { return guard; }
    const Expression& getCost() const { return cost; }
    Expression& getCostMut() { return cost; }
};


class RuleRhs {
    LocationIdx loc;
    UpdateMap update;

public:
    RuleRhs(LocationIdx loc, UpdateMap update) : loc(loc), update(update) {}

    LocationIdx getLoc() const { return loc; }
    const UpdateMap& getUpdate() const { return update; }
    UpdateMap& getUpdateMut() { return update; }
};


class LinearRule;

/**
 * A general rule, consisting of a left-hand side with location, guard and cost
 * and several (but at least one) right-hand sides, each with location and update.
 *
 * The lhs/rhs locations must not (and cannot) be changed, as they are tied to the graph in ITSProblem.
 * The content of the rule (guard/cost/update) may be modified (e.g. via getGuardMut()).
 */
class Rule {
private:
    RuleLhs lhs;
    std::vector<RuleRhs> rhss;

public:
    Rule(RuleLhs lhs, std::vector<RuleRhs> rhss);

    // constructors for linear rules
    Rule(RuleLhs lhs, RuleRhs rhs);
    Rule(LocationIdx lhsLoc, GuardList guard, Expression cost, LocationIdx rhsLoc, UpdateMap update);

    // constructs an empty rule (guard/update empty, cost 0)
    static LinearRule dummyRule(LocationIdx lhsLoc, LocationIdx rhsLoc);
    bool isDummyRule() const;

    const RuleLhs& getLhs() const { return lhs; }
    const std::vector<RuleRhs>& getRhss() const { return rhss; }

    // query lhs data
    LocationIdx getLhsLoc() const { return lhs.getLoc(); }
    const GuardList& getGuard() const { return lhs.getGuard(); }
    const Expression& getCost() const { return lhs.getCost(); }

    // lhs mutation
    GuardList& getGuardMut() { return lhs.getGuardMut(); }
    Expression& getCostMut() { return lhs.getCostMut(); }

    // iteration over right-hand sides
    const RuleRhs* rhsBegin() const { return &rhss.front(); }
    const RuleRhs* rhsEnd() const { return &rhss.back()+1; }
    RuleRhs* rhsBegin() { return &rhss.front(); }
    RuleRhs* rhsEnd() { return &rhss.back()+1; }
    size_t rhsCount() const { return rhss.size(); }

    // special methods for nonlinear rules (idx is an index to rhss)
    LocationIdx getRhsLoc(int idx) const { return rhss[idx].getLoc(); }
    const UpdateMap& getUpdate(int idx) const { return rhss[idx].getUpdate(); }
    UpdateMap& getUpdateMut(int idx) { return rhss[idx].getUpdateMut(); }

    // conversion to linear rule
    bool isLinear() const;
    LinearRule toLinear() const;

    // checks if lhs location coincides with _all_ rhs locations
    bool isSimpleLoop() const;

    // checks if lhs location coincides with at least _one_ rhs location
    bool hasSelfLoop() const; // TODO: is this ever used?

    // applies the given substitution to guard, cost, and the update's rhss (not to the update's lhss!)
    // Note: Result may be incorrect if an updated variable is updated (which is not checked!)
    // Note: It is always safe if only temporary variables are substituted.
    void applySubstitution(const GiNaC::exmap &subs);

    // Creates a new rule that only leads to the given location, the updates are cleared, guard/cost are kept
    LinearRule replaceRhssBySink(LocationIdx sink) const;

    // Removes all right-hand sides that lead to the given location, returns none if all rhss would be removed
    option<Rule> stripRhsLocation(LocationIdx toRemove) const;
};


/**
 * A linear rule is a rule with exactly one right-hand side.
 *
 * To simplify the code, linear rules are just a subclass of Rule,
 * so they use the same data representation (a vector of right-hand sides).
 * The result is that most of the code can work with the more general Rule,
 * at the cost of a minor performance loss (which is probably not critical).
 *
 * This class is only used to have more expressive function signatures.
 * If performance is critical, all uses of LinearRule can be replaced by Rule
 * together with assertions that the Rule is in fact linear.
 */
class LinearRule : public Rule {
public:
    LinearRule(RuleLhs lhs, RuleRhs rhs) : Rule(lhs, rhs) {}
    LinearRule(LocationIdx lhsLoc, GuardList guard, Expression cost, LocationIdx rhsLoc, UpdateMap update)
            : Rule(lhsLoc, guard, cost, rhsLoc, update) {}

    // special shorthands for linear rules, overwriting the general ones
    LocationIdx getRhsLoc() const { return Rule::getRhsLoc(0); }
    const UpdateMap& getUpdate() const { return Rule::getUpdate(0); }
    UpdateMap& getUpdateMut() { return Rule::getUpdateMut(0); }
};


/**
 * For debugging output (not very readable)
 */
std::ostream& operator<<(std::ostream &s, const Rule &rule);


#endif // RULE_H
