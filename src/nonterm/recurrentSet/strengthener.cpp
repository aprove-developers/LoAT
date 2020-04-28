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

#include "strengthener.hpp"
#include "templatebuilder.hpp"
#include "constraintsolver.hpp"
#include "guardcontextbuilder.hpp"

namespace strengthening {

    const option<LinearRule> Strengthener::apply(const LinearRule &rule, ITSProblem &its) {
        if (!rule.getGuard()->isLinear() || !rule.getUpdate().isLinear()) return {};
        const Strengthener strengthener(rule, its);
        const option<BoolExpr> &strengthened = strengthener.apply(rule.getGuard());
        if (strengthened) {
            const RuleLhs newLhs(rule.getLhsLoc(), strengthened.get(), rule.getCost());
            return {LinearRule(newLhs, rule.getRhss()[0])};
        }
        return {};
    }

    Strengthener::Strengthener(const Rule &rule, VariableManager &varMan): rule(rule), varMan(varMan) { }

    const option<BoolExpr> Strengthener::apply(const BoolExpr &guard) const {
        const GuardContext &guardCtx = GuardContextBuilder::build(guard, rule.getUpdates(), varMan);
        const Templates &templates = TemplateBuilder::build(guardCtx, rule, varMan);
        std::vector<ForAllExpr> constraints = ConstraintBuilder::buildSmtConstraints(templates, rule, guardCtx, varMan);
        auto trivial = [](const ForAllExpr &e){
            return e.expr == True;
        };
        for (auto it = constraints.begin(); it < constraints.end();) {
            if (trivial(*it)) {
                it = constraints.erase(it);
            } else {
                ++it;
            }
        }
        if (constraints.empty()) {
            return {};
        }
        const BoolExpr newInv = ConstraintSolver::solve(constraints, templates, varMan);
        if (newInv != True) {
            return {rule.getGuard() & newInv};
        }
        return {};
    }

}
