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

#include "preprocess.hpp"

#include "../expr/guardtoolbox.hpp"
#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"

using namespace std;


option<Rule> Preprocess::preprocessRule(const VarMan &varMan, const Rule &rule) {
    bool result = false;
    Rule oldRule = rule;
    option<Rule> newRule;

    // First try to find equalities (A <= B and B <= A become A == B).
    // We do this before simplifying by smt, as this might remove one of the two constraints
    // (if it is semantically implied) so we don't recognize the equality later on.
    newRule = GuardToolbox::makeEqualities(oldRule);
    if (newRule) {
        result = true;
        oldRule = newRule.get();
    }

    // Simplify with smt only once
    newRule = simplifyGuard(oldRule);
    if (newRule) {
        result = true;
        oldRule = newRule.get();
    }

    newRule = simplifyGuardBySmt(oldRule, varMan);
    if (newRule) {
        result = true;
        oldRule = newRule.get();
    }

    // The other steps are repeated (might not help very often, but is probably cheap enough)
    bool changed = false;
    do {
        changed = false;
        newRule = eliminateTempVars(varMan, oldRule);
        if (newRule) {
            changed = true;
            oldRule = newRule.get();
        }

        if (changed) {
            newRule = simplifyGuard(oldRule);
            if (newRule) {
                oldRule = newRule.get();
            }
        }

        newRule = removeTrivialUpdates(oldRule);
        if (newRule) {
            changed = true;
            oldRule = newRule.get();
        }

        result = result || changed;
    } while (changed);

    if (result) {
        return {oldRule};
    } else {
        return {};
    }
}


option<Rule> Preprocess::simplifyRule(const VarMan &varMan, const Rule &rule) {
    bool changed = false;
    Rule oldRule = rule;
    option<Rule> newRule;

    newRule = eliminateTempVars(varMan, oldRule);
    if (newRule) {
        changed = true;
        oldRule = newRule.get();
    }

    newRule = simplifyGuard(oldRule);
    if (newRule) {
        changed = true;
        oldRule = newRule.get();
    }

    newRule = removeTrivialUpdates(oldRule);
    if (newRule) {
        changed = true;
        oldRule = newRule.get();
    }

    if (changed) {
        return {oldRule};
    } else {
        return {};
    }
}


option<Rule> Preprocess::simplifyGuard(const Rule &rule) {
    Guard newGuard;

    for (const Rel &rel : rule.getGuard()) {
        // Skip trivially true constraints
        if (rel.isTriviallyTrue()) continue;

        // If a constraint is trivially false, we drop all other constraints
        if (rel.isTriviallyFalse()) {
            newGuard = {rel};
            break;
        }

        // Check if the constraint is syntactically implied by one of the other constraints.
        // Also check if one of the others is implied by the new constraint.
        bool implied = false;
        for (unsigned int i=0; i < newGuard.size(); ++i) {
            if (GuardToolbox::isTrivialImplication(newGuard.at(i), rel)) {
                implied = true;

            } else if (GuardToolbox::isTrivialImplication(rel, newGuard.at(i))) {
                // remove old constraint from newGuard, but preserve order
                newGuard.erase(newGuard.begin() + i);
                i--;
            }
        }

        if (!implied) {
            newGuard.push_back(rel);
        }
    }

    if (rule.getGuard().size() == newGuard.size()) {
        return {};
    } else {
        return {rule.withGuard(newGuard)};
    }
}


option<Rule> Preprocess::simplifyGuardBySmt(const Rule &rule, const VariableManager &varMan) {
    unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic<Subs>({rule.getGuard()}, {}), varMan);

    // iterates once over guard and drops constraints that are implied by previous constraints
    auto dropImplied = [&](const Guard &guard) {
        Guard res;
        for (const Rel &rel : guard) {
            solver->push();
            solver->add(!buildLit(rel));
            auto smtRes = solver->check();
            solver->pop();

            // unsat means that ex is implied by the previous constraints
            if (smtRes != Smt::Unsat) {
                res.push_back(rel);
                solver->add(rel);
            }
        }
        return res;
    };

    // iterated once, drop implied constraints
    Guard newGuard = dropImplied(rule.getGuard());

    // reverse the guard
    std::reverse(newGuard.begin(), newGuard.end());

    // iterate over the reversed guard, drop more implied constraints,
    // e.g. for "A > 0, A > 1" only the reverse iteration can remove "A > 0".
    solver->resetSolver();
    newGuard = dropImplied(newGuard);

    // reverse again to preserve original order
    std::reverse(newGuard.begin(), newGuard.end());
    if (rule.getGuard().size() == newGuard.size()) {
        return {};
    } else {
        return {rule.withGuard(newGuard)};
    }
}

option<Rule> Preprocess::removeTrivialUpdates(const Rule &rule) {
    bool changed = false;
    std::vector<RuleRhs> newRhss;
    for (const RuleRhs &rhs: rule.getRhss()) {
        Subs up = rhs.getUpdate();
        changed |= removeTrivialUpdates(up);
        newRhss.push_back(RuleRhs(rhs.getLoc(), up));
    }
    if (changed) {
        return {Rule(rule.getLhs(), newRhss)};
    } else {
        return {};
    }
}

bool Preprocess::removeTrivialUpdates(Subs &update) {
    stack<Var> remove;
    for (auto it : update) {
        if (it.second.equals(it.first)) {
            remove.push(it.first);
        }
    }
    if (remove.empty()) return false;
    while (!remove.empty()) {
        update.erase(remove.top());
        remove.pop();
    }
    return true;
}


/**
 * Returns the set of all variables that appear in the rhs of some update.
 * For an update x := a and x := x+a, this is {a} and {x,a}, respectively.
 */
static VarSet collectVarsInUpdateRhs(const Rule &rule) {
    VarSet varsInUpdate;
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        for (const auto &it : rhs->getUpdate()) {
            it.second.collectVars(varsInUpdate);
        }
    }
    return varsInUpdate;
}


option<Rule> Preprocess::eliminateTempVars(const VarMan &varMan, const Rule &rule) {
    //collect all variables that appear in the rhs of any update
    VarSet varsInUpdate = collectVarsInUpdateRhs(rule);

    //declare helper lambdas to filter variables, to be passed as arguments
    auto isTemp = [&](const Var &sym) {
        return varMan.isTempVar(sym);
    };
    auto isTempInUpdate = [&](const Var &sym) {
        return isTemp(sym) && varsInUpdate.count(sym) > 0;
    };
    auto isTempOnlyInGuard = [&](const Var &sym) {
        return isTemp(sym) && varsInUpdate.count(sym) == 0 && !rule.getCost().has(sym);
    };

    bool changed = false;
    Rule oldRule = rule;
    option<Rule> newRule;

    //equalities allow easy propagation, thus transform x <= y, x >= y into x == y
    newRule = GuardToolbox::makeEqualities(oldRule);
    if (newRule) {
        oldRule = newRule.get();
        changed = true;
    }
    //try to remove temp variables from the update by equality propagation (they are removed from guard and update)
    newRule = GuardToolbox::propagateEqualities(varMan, oldRule, GuardToolbox::ResultMapsToInt, isTempInUpdate);
    if (newRule) {
        oldRule = newRule.get();
        changed = true;
    }

    //try to remove all remaining temp variables (we do 2 steps to priorizie removing vars from the update)
    newRule = GuardToolbox::propagateEqualities(varMan, oldRule, GuardToolbox::ResultMapsToInt, isTemp);
    if (newRule) {
        oldRule = newRule.get();
        changed = true;
    }

    //note that propagation can alter the update, so we have to recompute
    //the variables that appear in the rhs of an update:
    varsInUpdate = collectVarsInUpdateRhs(rule);

    //now eliminate a <= x and replace a <= x, x <= b by a <= b for all free variables x where this is sound
    //(not sound if x appears in update or cost, since we then need the value of x)
    newRule = GuardToolbox::eliminateByTransitiveClosure(oldRule, true, isTempOnlyInGuard);
    if (newRule) {
        oldRule = newRule.get();
        changed = true;
    }

    if (changed) {
        return {oldRule};
    } else {
        return {};
    }
}
