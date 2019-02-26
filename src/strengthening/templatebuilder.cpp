//
// Created by ffrohn on 2/21/19.
//

#include <its/variablemanager.h>
#include "templatebuilder.h"

namespace strengthening {

    typedef TemplateBuilder Self;

    const Templates Self::build(const GuardContext &guardCtx, const RuleContext &ruleCtx) {
        return TemplateBuilder(guardCtx, ruleCtx).build();
    }

    Self::TemplateBuilder(const GuardContext &guardCtx, const RuleContext &ruleCtx):
            guardCtx(guardCtx), ruleCtx(ruleCtx) { }

    const Templates Self::build() const {
        Templates res = Templates();
        for (const Expression &g: guardCtx.todo) {
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
            res.insert(ruleCtx.varMan.getVarIdx(sym));
        }
        // Compute the closure of res under all updates and the guard
        std::set<VariableIdx> todo = res;
        while (!todo.empty()) {
            ExprSymbolSet next;
            for (const VariableIdx &var : todo) {
                for (const RuleRhs &rhs: ruleCtx.rule.getRhss()) {
                    const UpdateMap &update = rhs.getUpdate();
                    auto it = update.find(var);
                    if (it != update.end()) {
                        const ExprSymbolSet &rhsVars = it->second.getVariables();
                        next.insert(rhsVars.begin(), rhsVars.end());
                    }
                }
                for (const Expression &g: guardCtx.guard) {
                    const ExprSymbolSet &gVars = g.getVariables();
                    if (gVars.find(ruleCtx.varMan.getVarSymbol(var)) != gVars.end()) {
                        next.insert(gVars.begin(), gVars.end());
                    }
                }
            }
            todo.clear();
            for (const ExprSymbol &sym : next) {
                const VariableIdx &var = ruleCtx.varMan.getVarIdx(sym);
                if (res.count(var) == 0) {
                    todo.insert(var);
                }
            }
            // collect all variables from every iteration
            res.insert(todo.begin(), todo.end());
        }
        ExprSymbolSet symbols;
        for (const VariableIdx &x: res) {
            symbols.insert(ruleCtx.varMan.getVarSymbol(x));
        }
        return symbols;
    }

    const Templates::Template Self::buildTemplate(const ExprSymbolSet &vars) const {
        ExprSymbolSet params;
        const ExprSymbol &c0 = ruleCtx.varMan.getVarSymbol(ruleCtx.varMan.addFreshVariable("c0"));
        params.insert(c0);
        Expression res = c0;
        for (const ExprSymbol &x: vars) {
            const ExprSymbol &param = ruleCtx.varMan.getVarSymbol(ruleCtx.varMan.addFreshVariable("c"));
            params.insert(param);
            res = res + (x * param);
        }
        return Templates::Template(res <= 0, vars, std::move(params));
    }

}
