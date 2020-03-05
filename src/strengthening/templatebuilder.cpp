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
        for (const Rel &rel: guardCtx.todo) {
            const VarSet &varSymbols = util::RelevantVariables::find(
                    {rel},
                    ruleCtx.updates,
                    guardCtx.guard,
                    ruleCtx.varMan);
            res.add(buildTemplate(varSymbols));
        }
        return res;
    }

    const Templates::Template TemplateBuilder::buildTemplate(const VarSet &vars) const {
        VarSet params;
        const Var &c0 = ruleCtx.varMan.getVarSymbol(ruleCtx.varMan.addFreshVariable("c0"));
        params.insert(c0);
        Expr res = c0;
        for (const Var &x: vars) {
            const Var &param = ruleCtx.varMan.getVarSymbol(ruleCtx.varMan.addFreshVariable("c"));
            params.insert(param);
            res = res + (x * param);
        }
        return Templates::Template(res <= 0, vars, std::move(params));
    }

}
