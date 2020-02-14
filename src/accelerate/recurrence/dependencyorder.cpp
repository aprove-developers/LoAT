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

#include "dependencyorder.hpp"

using namespace std;

struct PartialResult {
    std::vector<VariableIdx> ordering; // might not contain all variables (hence partial)
    std::set<VariableIdx> ordered; // set of all variables occurring in ordering
};


/**
 * The core implementation.
 * Successively adds variables to the ordering for which all dependencies are
 * already ordered. Stops if this is no longer possible (we are either done
 * or there are conflicting variables depending on each other).
 */
static void findOrderUntilConflicting(const VarMan &varMan, const UpdateMap &update, PartialResult &res) {
    bool changed = true;

    while (changed && res.ordering.size() < update.size()) {
        changed = false;

        for (const auto &up : update) {
            if (res.ordered.count(up.first) > 0) continue;

            //check if all variables on update rhs are already processed
            bool ready = true;
            for (const ExprSymbol &sym : up.second.getVariables()) {
                VariableIdx var = varMan.getVarIdx(sym);
                if (var != up.first && update.count(var) > 0 && res.ordered.count(var) == 0) {
                    ready = false;
                    break;
                }
            }

            if (ready) {
                res.ordered.insert(up.first);
                res.ordering.push_back(up.first);
                changed = true;
            }
        }
    }
}

option<vector<VariableIdx>> DependencyOrder::findOrder(const VarMan &varMan, const UpdateMap &update) {
    PartialResult res;
    findOrderUntilConflicting(varMan, update, res);

    if (res.ordering.size() == update.size()) {
        return res.ordering;
    }

    return {};
}


option<vector<VariableIdx>> DependencyOrder::findOrderWithHeuristic(const VarMan &varMan, UpdateMap &update,
                                                                      GuardList &guard)
{
    // order variables until a conflict is reached
    PartialResult res;
    findOrderUntilConflicting(varMan, update, res);

    // check if we are done
    if (res.ordering.size() == update.size()) {
        return res.ordering;
    }

    // If not all dependencies could be resolved, try to add constraints to the guard to make things easier.
    // e.g. for A'=A+B, B'=A+B we add the constraint A==B and can thus simplify to A'=A+A, B'=A+A.

    // Note that this is only possible if A'==B' follows from A==B. This is always the case if the two right-hand
    // sides are equal, as in the example above (A+B and A+B). It is also the case if the right-hand sides are
    // equal after substituting A/B (to enforce A==B).

    // To see that this is condition is required, consider A'=B+1, B'=A+2.
    // Note that A'==B' is *not* implied by A==B. Hence using A'=A+1, B'=A+2 is not correct!

    // Choose one of the remaining variables
    auto notOrdered = [&](const pair<VariableIdx,Expression> &it){ return res.ordered.count(it.first) == 0; };
    auto it = std::find_if(update.begin(), update.end(), notOrdered);
    assert(it != update.end());

    ExprSymbol targetSym = varMan.getVarSymbol(it->first);
    Expression targetRhs = it->second;

    // Build a substitution that replaces all remaining variables x by var.
    // To ensure soundness, constraints "x == var" are added to the guard.
    GiNaC::exmap subs;
    for (const auto &up : update) {
        if (res.ordered.count(up.first) > 0) continue;
        subs[varMan.getVarSymbol(up.first)] = targetSym;
        guard.push_back(varMan.getVarSymbol(up.first) == targetSym);
    }

    // Apply the substitution to all remaining updates.
    // To ensure soundness, check that the resulting updates are all equal (to targetRhs)
    targetRhs.applySubs(subs);
    for (auto &up : update) {
        if (res.ordered.count(up.first) > 0) continue;
        up.second.applySubs(subs);
        if (!up.second.is_equal(targetRhs)) {
            // optimization cannot be applied, give up
            return {};
        }
    }

    // Now an order is trivial to find (any order will do)
    findOrderUntilConflicting(varMan, update, res);
    assert (res.ordering.size() == update.size());
    return res.ordering;
}
