/*  This file is part of LoAT.
 *  Copyright (c) 2019 Florian Frohn
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

#include "relevantvariables.hpp"
#include "../its/variablemanager.hpp"

namespace util {

    const ExprSymbolSet RelevantVariables::find(
            const GuardList &constraints,
            const std::vector<ExprMap> &updates,
            const GuardList &guard,
            const VariableManager &varMan) {
        std::set<VariableIdx> res;
        // Add all variables appearing in the guard
        ExprSymbolSet guardVariables;
        for (const Rel &rel: constraints) {
            const ExprSymbolSet &relVars = rel.getVariables();
            guardVariables.insert(relVars.begin(), relVars.end());
        }
        for (const ExprSymbol &sym : guardVariables) {
            res.insert(varMan.getVarIdx(sym));
        }
        // Compute the closure of res under all updates and the guard
        std::set<VariableIdx> todo = res;
        while (!todo.empty()) {
            ExprSymbolSet next;
            for (const VariableIdx &var : todo) {
                const ExprSymbol &x = varMan.getVarSymbol(var);
                for (const ExprMap &up: updates) {
                    auto it = up.find(x);
                    if (it != up.end()) {
                        const ExprSymbolSet &rhsVars = Expression(it->second).getVariables();
                        next.insert(rhsVars.begin(), rhsVars.end());
                    }
                }
                for (const Rel &rel: guard) {
                    const ExprSymbolSet &relVars = rel.getVariables();
                    if (relVars.find(varMan.getVarSymbol(var)) != relVars.end()) {
                        next.insert(relVars.begin(), relVars.end());
                    }
                }
            }
            todo.clear();
            for (const ExprSymbol &sym : next) {
                const VariableIdx &var = varMan.getVarIdx(sym);
                if (res.count(var) == 0) {
                    todo.insert(var);
                }
            }
            // collect all variables from every iteration
            res.insert(todo.begin(), todo.end());
        }
        ExprSymbolSet symbols;
        for (const VariableIdx &x: res) {
            symbols.insert(varMan.getVarSymbol(x));
        }
        return symbols;
    }

    const ExprSymbolSet RelevantVariables::find(
            const GuardList &constraints,
            const std::vector<UpdateMap> &updateMaps,
            const GuardList &guard,
            const VariableManager &varMan) {
        std::vector<ExprMap> updates;
        for (const UpdateMap &up: updateMaps) {
            updates.push_back(up.toSubstitution(varMan));
        }
        return RelevantVariables::find(constraints, updates, guard, varMan);
    }

    const ExprSymbolSet RelevantVariables::find(
            const GuardList &constraints,
            const std::vector<RuleRhs> &rhss,
            const GuardList &guard,
            const VariableManager &varMan) {
        std::vector<ExprMap> updates;
        for (const RuleRhs &rhs: rhss) {
            updates.push_back(rhs.getUpdate().toSubstitution(varMan));
        }
        return RelevantVariables::find(constraints, updates, guard, varMan);
    }

}
