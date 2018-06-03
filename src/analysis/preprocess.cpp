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

#include "preprocess.h"

#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "z3/z3toolbox.h"
#include "util/timeout.h"

using namespace std;


// FIXME: Remove this! Instead provide method to add the "cost >= 0" constraint, and check if it is implied there
bool Preprocess::tryToRemoveCost(GuardList &guard) {
    if (guard.empty()) return false;
    GuardList realGuard(guard.begin(),guard.end()-1);
    if (Z3Toolbox::isValidImplication(realGuard, guard.back())) {
        guard.pop_back();
        return true;
    }
    return false;
}


bool Preprocess::simplifyRule(const VarMan &varMan, Rule &rule) {
    bool changed, result;

    //do removeWeakerGuards only once, as this involves z3 and is potentially slow
    result = removeTrivialGuards(rule.getGuardMut());
    result = removeWeakerGuards(rule.getGuardMut()) || result;

    //all other steps are repeated
    do {
        // simplify guard
        changed = removeTrivialGuards(rule.getGuardMut());
        changed = eliminateFreeVars(varMan,rule) || changed;

        // simplify updates
        for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
            changed = removeTrivialUpdates(varMan, rhs->getUpdateMut()) || changed;
        }

        result = result || changed;
    } while (changed);

    return result;
}


bool Preprocess::removeTrivialGuards(GuardList &guard) {
    bool changed = false;
    for (int i=0; i < guard.size(); ++i) {
        if (Relation::isEquality(guard[i])) continue;

        Expression lessEq = Relation::toLessEq(guard[i]);
        if (Relation::isTrivialLessEqInequality(lessEq)) {
            guard.erase(guard.begin() + i);
            i--;
            changed = true;
        }
    }
    return changed;
}


// TODO: This method is extremely expensive (it does O(n^2) SMT queries), even though its not very effective!
bool Preprocess::removeWeakerGuards(GuardList &guard) {
    auto tout = Timeout::create(3); //this function is very expensive, limit the time spent here
    set<int> remove;
    //check for every pair of expressions if one implies the other
    for (int i=0; i < guard.size(); ++i) {
        if (Timeout::over(tout)) goto timeout;
        if (remove.count(i) > 0) continue;
        for (int j=0; j < guard.size(); ++j) {
            if (i == j || remove.count(j) > 0) continue;
            if (Z3Toolbox::isValidImplication({guard[i]}, guard[j])) {
                remove.insert(j);
            }
        }
    }
timeout:
    if (remove.empty()) return false;
    //remove in reverse order to keep indices valid until they are removed
    for (auto it = remove.rbegin(); it != remove.rend(); ++it) {
        guard.erase(guard.begin() + *it);
    }
    return true;
}


bool Preprocess::removeTrivialUpdates(const VarMan &varMan, UpdateMap &update) {
    stack<VariableIdx> remove;
    for (auto it : update) {
        if (it.second.equalsVariable(varMan.getGinacSymbol(it.first))) {
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


bool Preprocess::eliminateTempVars(const VarMan &varMan, Rule &rule) {
    bool changed = false;

    //collect all variables that appear in the rhs of any update
    ExprSymbolSet varsInUpdate;
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        for (const auto &it : rhs->getUpdate()) {
            it.second.collectVariables(varsInUpdate);
        }
    }

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

    //now eliminate a <= x and replace a <= x, x <= b by a <= b for all free variables x where this is sound
    //(not sound if x appears in update or cost, since we then need the value of x)
    changed |= GuardToolbox::eliminateByTransitiveClosure(rule.getGuardMut(), true, isTempOnlyInGuard);

    return changed;
}
