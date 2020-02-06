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
#include "../expr/relation.hpp"
#include "../z3/z3toolbox.hpp"
#include "../z3/z3solver.hpp"
#include "../util/timeout.hpp"

using namespace std;


bool Preprocess::preprocessRule(const VarMan &varMan, Rule &rule) {
    bool changed;
    bool result = false;

    // First try to find equalities (A <= B and B <= A become A == B).
    // We do this before simplifying by smt, as this might remove one of the two constraints
    // (if it is semantically implied) so we don't recognize the equality later on.
    result |= GuardToolbox::makeEqualities(rule.getGuardMut());

    // Simplify with smt only once
    result |= simplifyGuard(rule.getGuardMut());
    result |= simplifyGuardBySmt(rule.getGuardMut());

    // The other steps are repeated (might not help very often, but is probably cheap enough)
    do {
        changed = eliminateTempVars(varMan,rule);

        if (changed) {
            changed |= simplifyGuard(rule.getGuardMut());
        }

        for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
            changed |= removeTrivialUpdates(varMan, rhs->getUpdateMut());
        }

        result = result || changed;
    } while (changed);

    return result;
}


bool Preprocess::simplifyRule(const VarMan &varMan, Rule &rule) {
    bool changed = false;

    changed |= eliminateTempVars(varMan, rule);
    changed |= simplifyGuard(rule.getGuardMut());

    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        changed = removeTrivialUpdates(varMan, rhs->getUpdateMut()) || changed;
    }

    return changed;
}


bool Preprocess::simplifyGuard(GuardList &guard) {
    GuardList newGuard;

    for (const Expression &ex : guard) {
        // Skip trivially true constraints
        auto optTrivial = Relation::checkTrivial(ex);
        if (optTrivial && optTrivial.get()) continue;

        // If a constraint is trivially false, we drop all other constraints
        if (optTrivial && !optTrivial.get()) {
            newGuard = {ex};
            break;
        }

        // Check if the constraint is syntactically implied by one of the other constraints.
        // Also check if one of the others is implied by the new constraint.
        bool implied = false;
        for (unsigned int i=0; i < newGuard.size(); ++i) {
            if (GuardToolbox::isTrivialImplication(newGuard.at(i), ex)) {
                implied = true;

            } else if (GuardToolbox::isTrivialImplication(ex, newGuard.at(i))) {
                // remove old constraint from newGuard, but preserve order
                newGuard.erase(newGuard.begin() + i);
                i--;
            }
        }

        if (!implied) {
            newGuard.push_back(ex);
        }
    }

    guard.swap(newGuard);
    return guard.size() != newGuard.size();
}


bool Preprocess::simplifyGuardBySmt(GuardList &guard) {
    GuardList newGuard;
    Z3Context context;
    Z3Solver solver(context);

    // iterates once over guard and drops constraints that are implied by previous constraints
    auto dropImplied = [&]() {
        for (const Expression &ex : guard) {
            solver.push();
            solver.add( ! ex.toZ3(context) );
            auto z3res = solver.check();
            solver.pop();

            // unsat means that ex is implied by the previous constraints
            if (z3res != z3::unsat) {
                newGuard.push_back(ex);
                solver.add(ex.toZ3(context));
            }
        }
    };

    // iterated once, drop implied constraints
    dropImplied();

    // reverse the guard
    std::reverse(newGuard.begin(), newGuard.end());
    guard.swap(newGuard);

    // iterate over the reversed guard, drop more implied constraints,
    // e.g. for "A > 0, A > 1" only the reverse iteration can remove "A > 0".
    newGuard.clear();
    solver.reset();
    dropImplied();

    // reverse again to preserve original order
    std::reverse(newGuard.begin(), newGuard.end());
    guard.swap(newGuard);
    return guard.size() != newGuard.size();
}


bool Preprocess::removeTrivialUpdates(const VarMan &varMan, UpdateMap &update) {
    stack<VariableIdx> remove;
    for (auto it : update) {
        if (it.second.equalsVariable(varMan.getVarSymbol(it.first))) {
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
static ExprSymbolSet collectVarsInUpdateRhs(const Rule &rule) {
    ExprSymbolSet varsInUpdate;
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        for (const auto &it : rhs->getUpdate()) {
            it.second.collectVariables(varsInUpdate);
        }
    }
    return varsInUpdate;
}


bool Preprocess::eliminateTempVars(const VarMan &varMan, Rule &rule) {
    bool changed = false;

    //collect all variables that appear in the rhs of any update
    ExprSymbolSet varsInUpdate = collectVarsInUpdateRhs(rule);

    //declare helper lambdas to filter variables, to be passed as arguments
    auto isTemp = [&](const ExprSymbol &sym) {
        return varMan.isTempVar(sym);
    };
    auto isTempInUpdate = [&](const ExprSymbol &sym) {
        return isTemp(sym) && varsInUpdate.count(sym) > 0;
    };
    auto isTempOnlyInGuard = [&](const ExprSymbol &sym) {
        return isTemp(sym) && varsInUpdate.count(sym) == 0 && !rule.getCost().has(sym);
    };

    //equalities allow easy propagation, thus transform x <= y, x >= y into x == y
    changed |= GuardToolbox::makeEqualities(rule.getGuardMut());

    //try to remove temp variables from the update by equality propagation (they are removed from guard and update)
    changed |= GuardToolbox::propagateEqualities(varMan, rule, GuardToolbox::ResultMapsToInt, isTempInUpdate);

    //try to remove all remaining temp variables (we do 2 steps to priorizie removing vars from the update)
    changed |= GuardToolbox::propagateEqualities(varMan, rule, GuardToolbox::ResultMapsToInt, isTemp);

    //note that propagation can alter the update, so we have to recompute
    //the variables that appear in the rhs of an update:
    varsInUpdate = collectVarsInUpdateRhs(rule);

    //now eliminate a <= x and replace a <= x, x <= b by a <= b for all free variables x where this is sound
    //(not sound if x appears in update or cost, since we then need the value of x)
    changed |= GuardToolbox::eliminateByTransitiveClosure(rule.getGuardMut(), true, isTempOnlyInGuard);

    return changed;
}
