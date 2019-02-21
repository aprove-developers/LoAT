//
// Created by ffrohn on 2/21/19.
//

#include <its/variablemanager.h>
#include <expr/expression.h>
#include "templatebuilder.h"
#include "types.h"
#include "templates.h"

namespace strengthening {

    typedef TemplateBuilder Self;

    Self::TemplateBuilder(const GuardList &constraints, const Rule &rule, VariableManager& varMan):
            constraints(constraints), rule(rule), varMan(varMan) { }

    const Templates Self::buildTemplates() const {
        Templates res = Templates();
        for (const Expression &g: constraints) {
            const ExprSymbolSet &varSymbols = findRelevantVariables(g);
            const Templates::Template &t = buildTemplate(varSymbols);
            res.add(t);
        }
        return res;
    }

    const ExprSymbolSet Self::findRelevantVariables(const Expression &c) const {
        std::set<VariableIdx> res;
        // Add all variables appearing in the guard
        const ExprSymbolSet &guardVariables = c.getVariables();
        for (const ExprSymbol &sym : guardVariables) {
            res.insert(varMan.getVarIdx(sym));
        }
        // Compute the closure of res under all updates and the guard
        std::set<VariableIdx> todo = res;
        while (!todo.empty()) {
            ExprSymbolSet next;
            for (const VariableIdx &var : todo) {
                for (const RuleRhs &rhs: rule.getRhss()) {
                    const UpdateMap &update = rhs.getUpdate();
                    auto it = update.find(var);
                    if (it != update.end()) {
                        const ExprSymbolSet &rhsVars = it->second.getVariables();
                        next.insert(rhsVars.begin(), rhsVars.end());
                    }
                }
                for (const Expression &g: rule.getGuard()) {
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

    const Templates::Template Self::buildTemplate(const ExprSymbolSet &vars) const {
        ExprSymbolSet params;
        const ExprSymbol &c0 = varMan.getVarSymbol(varMan.addFreshVariable("c0"));
        params.insert(c0);
        Expression res = c0;
        for (const ExprSymbol &x: vars) {
            const ExprSymbol &param = varMan.getVarSymbol(varMan.addFreshVariable("c"));
            params.insert(param);
            res = res + (x * param);
        }
        return Templates::Template(std::move(res), vars, std::move(params));
    }

}
