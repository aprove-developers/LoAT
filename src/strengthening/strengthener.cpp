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

    const std::vector<LinearRule> Strengthener::apply(const LinearRule &rule, ITSProblem &its) {
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
        const std::vector<GuardList> &strengthened = strengthener.apply(rule.getGuard());
        std::vector<LinearRule> rules;
        for (const GuardList &g: strengthened) {
            const RuleLhs newLhs(rule.getLhsLoc(), g, rule.getCost());
            rules.emplace_back(LinearRule(newLhs, rule.getRhss()[0]));
        }
        return rules;
    }

    Strengthener::Strengthener(const RuleContext &ruleCtx): ruleCtx(ruleCtx) { }

    const std::vector<GuardList> Strengthener::apply(const GuardList &guard) const {
        std::vector<GuardList> res;
        const option<GuardContext> &guardCtx = GuardContextBuilder::build(guard, ruleCtx.updates, ruleCtx.varMan);
        if (!guardCtx) {
            return res;
        }
        const Templates &templates = TemplateBuilder::build(guardCtx.get(), ruleCtx);
        const MaxSmtConstraints &maxSmtConstraints = ConstraintBuilder::buildMaxSmtConstraints(templates, ruleCtx, guardCtx.get());
        if (maxSmtConstraints.hard == True) {
            return {};
        }
        const option<Invariants> &newInv = ConstraintSolver::solve(ruleCtx, maxSmtConstraints, templates);
        if (newInv) {
            GuardList newGuard(guardCtx->guard);
            newGuard.insert(
                    newGuard.end(),
                    newInv.get().invariants.begin(),
                    newInv.get().invariants.end());
            GuardList pseudoInvariantsValid(newGuard);
            pseudoInvariantsValid.insert(
                    pseudoInvariantsValid.end(),
                    newInv.get().pseudoInvariants.begin(),
                    newInv.get().pseudoInvariants.end());
            res.emplace_back(pseudoInvariantsValid);
        }
        return res;
    }

}
