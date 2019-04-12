//
// Created by ffrohn on 2/21/19.
//

#include "../its/variablemanager.hpp"
#include "../util/relevantvariables.hpp"
#include "templatebuilder.hpp"

namespace strengthening {

    const Templates TemplateBuilder::build(const GuardContext &guardCtx, const RuleContext &ruleCtx) {
        return TemplateBuilder(guardCtx, ruleCtx).build();
    }

    TemplateBuilder::TemplateBuilder(const GuardContext &guardCtx, const RuleContext &ruleCtx):
            guardCtx(guardCtx), ruleCtx(ruleCtx) { }

    const Templates TemplateBuilder::build() const {
        Templates res = Templates();
        for (const Expression &g: guardCtx.todo) {
            const ExprSymbolSet &varSymbols = util::RelevantVariables::find(
                    {g},
                    ruleCtx.updates,
                    guardCtx.guard,
                    ruleCtx.varMan);
            const Templates::Template &t = buildTemplate(varSymbols);
            res.add(t);
        }
        return res;
    }

    const Templates::Template TemplateBuilder::buildTemplate(const ExprSymbolSet &vars) const {
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
