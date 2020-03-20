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
#include "rulecontextbuilder.hpp"
#include "guardcontextbuilder.hpp"

namespace strengthening {

    const option<LinearRule> Strengthener::apply(const LinearRule &rule, ITSProblem &its) {
        for (const Rel &rel: rule.getGuard()) {
            if (!rel.isLinear()) {
                return {};
            }
        }
        for (const auto &p: rule.getUpdate()) {
            if (!p.second.isLinear()) {
                return {};
            }
        }
        const RuleContext &ruleCtx = RuleContextBuilder::build(rule, its);
        const Strengthener strengthener(ruleCtx);
        const option<Guard> &strengthened = strengthener.apply(rule.getGuard());
        if (strengthened) {
            const RuleLhs newLhs(rule.getLhsLoc(), strengthened.get(), rule.getCost());
            return {LinearRule(newLhs, rule.getRhss()[0])};
        }
        return {};
    }

    Strengthener::Strengthener(const RuleContext &ruleCtx): ruleCtx(ruleCtx) { }

    const option<Guard> Strengthener::apply(const Guard &guard) const {
        const GuardContext &guardCtx = GuardContextBuilder::build(guard, ruleCtx.updates, ruleCtx.varMan);
        const Templates &templates = TemplateBuilder::build(guardCtx, ruleCtx);
        const BoolExpr &constraints = ConstraintBuilder::buildSmtConstraints(templates, ruleCtx, guardCtx);
        if (constraints == True) {
            return {};
        }
        const option<Guard> &newInv = ConstraintSolver::solve(ruleCtx, constraints, templates);
        if (newInv) {
            Guard newGuard(guardCtx.guard);
            newGuard.insert(
                    newGuard.end(),
                    newInv.get().begin(),
                    newInv.get().end());
            return {newGuard};
        }
        return {};
    }

}
