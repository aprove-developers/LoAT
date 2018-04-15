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

#include "itrs.h"
#include "flowgraph.h"
#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "z3/z3toolbox.h"
#include "timeout.h"

using namespace std;


bool Preprocess::tryToRemoveCost(const ITRSProblem &itrs, GuardList &guard) {
    if (guard.empty()) return false;
    GuardList realGuard(guard.begin(),guard.end()-1);
    if (Z3Toolbox::isValidImplication(realGuard, guard.back())) {
        guard.pop_back();
        return true;
    }
    return false;
}


bool Preprocess::simplifyTransition(const ITRSProblem &itrs, Transition &trans) {
    bool result = false;
    bool changed;

    //do removeWeakerGuards only once, as this involves z3 and is potentially slow
    result = removeTrivialGuards(trans.guard);
    result = removeWeakerGuards(trans.guard) || result;

    //all other steps are repeated
    do {
        changed = removeTrivialGuards(trans.guard);
        changed = eliminateFreeVars(itrs,trans) || changed;
        changed = removeTrivialUpdates(itrs,trans.update) || changed;
        result = result || changed;
    } while (changed);
    return result;
}


bool Preprocess::removeTrivialGuards(GuardList &guard) {
    bool changed = false;
    for (int i=0; i < guard.size(); ++i) {
        if (Relation::isEquality(guard[i])) continue;

        Expression lessEq = Relation::transformInequalityLessEq(guard[i]);
        if (Relation::isTrivialLessEqInequality(lessEq)) {
            guard.erase(guard.begin() + i);
            i--;
            changed = true;
        }
    }
    return changed;
}


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


bool Preprocess::removeTrivialUpdates(const ITRSProblem &itrs, UpdateMap &update) {
    stack<VariableIndex> remove;
    for (auto it : update) {
        if (it.second.equalsVariable(itrs.getGinacSymbol(it.first))) {
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


bool Preprocess::eliminateFreeVars(const ITRSProblem &itrs, Transition &trans) {
    bool result = false; //final modification flag
    bool changed; //intermediate modification flag

    do {
        //equalities allow easy propagation, thus transform x <= y, x >= y into x == y
        changed = GuardToolbox::findEqualities(trans.guard);
        if (result && !changed) break;

        //helpers for guard preprocessing
        ExprSymbolSet varsInUpdate;
        auto sym_is_free = [&](const ExprSymbol &sym){ return itrs.isFreeVar(itrs.getVarindex(sym.get_name())); };
        auto free_in_update = [&](const ExprSymbol &sym){ return sym_is_free(sym) && varsInUpdate.count(sym) > 0; };

        //remove free variables from update rhs (varsInUpdate, e.g. x <- free with free == x+1). Repeat for transitive closure.
        GiNaC::exmap equalSubs;
        do {
            varsInUpdate.clear();
            for (auto it : trans.update) it.second.collectVariables(varsInUpdate);

            equalSubs.clear();
            changed = GuardToolbox::propagateEqualities(itrs,trans.guard,GuardToolbox::NoCoefficients,GuardToolbox::NoFreeOnRhs,&equalSubs,free_in_update) || changed;
            for (auto &it : trans.update) it.second = it.second.subs(equalSubs);
            trans.cost = trans.cost.subs(equalSubs);
        } while (!equalSubs.empty());

        //try to remove free variables from equalities
        equalSubs.clear();
        changed = GuardToolbox::propagateEqualities(itrs,trans.guard,GuardToolbox::NoCoefficients,GuardToolbox::NoFreeOnRhs,&equalSubs,sym_is_free) || changed;
        for (auto &it : trans.update) it.second = it.second.subs(equalSubs);
        trans.cost = trans.cost.subs(equalSubs);

        //find all free variables that do not occur in update and cost
        auto sym_is_free_onlyguard = [&](const ExprSymbol &sym){ return sym_is_free(sym) && varsInUpdate.count(sym) == 0 && !trans.cost.has(sym); };

        //now eliminate a <= x and replace a <= x, x <= b by a <= b for all free variables x where this is sound
        changed = GuardToolbox::eliminateByTransitiveClosure(trans.guard,itrs.getGinacVarList(),true,sym_is_free_onlyguard) || changed;

        result = result || changed;
    } while (changed);
    return result;
}
