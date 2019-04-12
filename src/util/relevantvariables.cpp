//
// Created by ffrohn on 3/14/19.
//

#include "relevantvariables.hpp"
#include "../its/variablemanager.hpp"

namespace util {

    const ExprSymbolSet RelevantVariables::find(
            const GuardList &constraints,
            const std::vector<GiNaC::exmap> &updates,
            const GuardList &guard,
            const VariableManager &varMan) {
        std::set<VariableIdx> res;
        // Add all variables appearing in the guard
        ExprSymbolSet guardVariables;
        for (const Expression &c: constraints) {
            const ExprSymbolSet &cVariables = c.getVariables();
            guardVariables.insert(cVariables.begin(), cVariables.end());
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
                for (const GiNaC::exmap &up: updates) {
                    auto it = up.find(x);
                    if (it != up.end()) {
                        const ExprSymbolSet &rhsVars = Expression(it->second).getVariables();
                        next.insert(rhsVars.begin(), rhsVars.end());
                    }
                }
                for (const Expression &g: guard) {
                    const ExprSymbolSet &gVars = g.getVariables();
                    if (gVars.find(varMan.getVarSymbol(var)) != gVars.end()) {
                        next.insert(gVars.begin(), gVars.end());
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
        std::vector<GiNaC::exmap> updates;
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
        std::vector<GiNaC::exmap> updates;
        for (const RuleRhs &rhs: rhss) {
            updates.push_back(rhs.getUpdate().toSubstitution(varMan));
        }
        return RelevantVariables::find(constraints, updates, guard, varMan);
    }

}
