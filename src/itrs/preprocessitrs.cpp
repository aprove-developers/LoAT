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

#include "preprocessitrs.h"

#include "itrsproblem.h"
#include "recursiongraph.h"
#include "guardtoolbox.h"
#include "z3toolbox.h"
#include "timeout.h"

using namespace std;


bool PreprocessITRS::simplifyRightHandSide(const ITRSProblem &itrs, RightHandSide &rhs) {
    bool result = false;
    bool changed;

    //do removeWeakerGuards only once, as this involves z3 and is potentially slow
    result = removeTrivialGuards(rhs.guard);
    result = removeWeakerGuards(rhs.guard) || result;

    //all other steps are repeated
    do {
        changed = removeTrivialGuards(rhs.guard);
        changed = eliminateFreeVars(itrs,rhs) || changed;
        result = result || changed;
    } while (changed);
    return result;
}


bool PreprocessITRS::removeTrivialGuards(TT::ExpressionVector &guard) {
    bool changed = false;
    for (int i=0; i < guard.size(); ++i) {
        if (GuardToolbox::isEquality(guard[i])) continue;
        if (GuardToolbox::isTrivialInequality(GuardToolbox::makeLessEqual(guard[i]))) {
            guard.erase(guard.begin() + i);
            i--;
            changed = true;
        }
    }
    return changed;
}


bool PreprocessITRS::removeWeakerGuards(TT::ExpressionVector &guard) {
    auto tout = Timeout::create(3); //this function is very expensive, limit the time spent here
    set<int> remove;
    //check for every pair of expressions if one implies the other
    for (int i=0; i < guard.size(); ++i) {
        // substitute function symbols by variables
        Expression asGiNaCI = guard[i].toGiNaC(true);
        if (Timeout::over(tout)) goto timeout;
        if (remove.count(i) > 0) continue;
        for (int j=0; j < guard.size(); ++j) {
            if (i == j || remove.count(j) > 0) continue;
            // substitute function symbols by variables
            Expression asGiNaCJ = guard[j].toGiNaC(true);
            if (Z3Toolbox::checkTautologicImplication({asGiNaCI},asGiNaCJ)) {
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


bool PreprocessITRS::eliminateFreeVars(const ITRSProblem &itrs, RightHandSide &rhs) {
    bool result = false; //final modification flag
    bool changed; //intermediate modification flag

    do {
        //equalities allow easy propagation, thus transform x <= y, x >= y into x == y
        changed = GuardToolbox::findEqualities(rhs.guard);
        if (result && !changed) break;

        //helpers for guard preprocessing
        ExprSymbolSet varsInUpdate;
        auto sym_is_free = [&](const ExprSymbol &sym){ return itrs.isFreeVariable(itrs.getVariableIndex(sym.get_name())); };
        auto free_in_update = [&](const ExprSymbol &sym){ return sym_is_free(sym) && varsInUpdate.count(sym) > 0; };

        //remove free variables from update rhs (varsInUpdate, e.g. x <- free with free == x+1). Repeat for transitive closure.
        GiNaC::exmap equalSubs;
        do {
            varsInUpdate.clear();
            rhs.term.collectVariables(varsInUpdate);

            equalSubs.clear();
            changed = GuardToolbox::propagateEqualities(itrs,rhs.guard,GuardToolbox::NoCoefficients,GuardToolbox::NoFreeOnRhs,&equalSubs,free_in_update) || changed;
            rhs.term = rhs.term.substitute(equalSubs);
            rhs.cost = rhs.cost.subs(equalSubs);
        } while (!equalSubs.empty());

        //try to remove free variables from equalities
        equalSubs.clear();
        changed = GuardToolbox::propagateEqualities(itrs,rhs.guard,GuardToolbox::NoCoefficients,GuardToolbox::NoFreeOnRhs,&equalSubs,sym_is_free) || changed;
        rhs.term = rhs.term.substitute(equalSubs);
        rhs.cost = rhs.cost.subs(equalSubs);

        //find all free variables that do not occur in update and cost
        auto sym_is_free_onlyguard = [&](const ExprSymbol &sym){ return sym_is_free(sym) && varsInUpdate.count(sym) == 0 && !rhs.cost.has(sym); };

        //now eliminate a <= x and replace a <= x, x <= b by a <= b for all free variables x where this is sound
        changed = GuardToolbox::eliminateByTransitiveClosure(itrs, rhs.guard,itrs.getGinacVarList(),true,sym_is_free_onlyguard) || changed;

        result = result || changed;
    } while (changed);

    return result;
}
