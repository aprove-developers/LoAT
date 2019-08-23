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

#include "../expr/relation.hpp"
#include "../z3/z3context.hpp"
#include "../z3/z3solver.hpp"
#include "strengthener.hpp"
#include "templatebuilder.hpp"
#include "constraintsolver.hpp"
#include "rulecontextbuilder.hpp"
#include "guardcontextbuilder.hpp"

namespace strengthening {


    const std::vector<LinearRule> Strengthener::apply(const LinearRule &rule, ITSProblem &its) {
        std::stack<GuardList> todo;
        todo.push(rule.getGuard());
        std::vector<GuardList> res;
        const RuleContext &ruleCtx = RuleContextBuilder::build(rule, its);
        const Strengthener strengthener(ruleCtx);
        unsigned int i = 0;
        while (!todo.empty() && i < 10) {
            i++;
            const GuardList &current = todo.top();
            if (Timeout::soft()) {
                return {};
            }
            const std::vector<GuardList> &strengthened = strengthener.apply(current);
            if (!strengthened.empty()) {
                todo.pop();
                for (const GuardList &s: strengthened) {
                    todo.push(s);
                }
            } else {
                if (current != rule.getGuard()) {
                    res.push_back(current);
                }
                todo.pop();
            }
        }
        std::vector<LinearRule> rules;
        for (const GuardList &g: res) {
            const RuleLhs newLhs(rule.getLhsLoc(), g, rule.getCost());
            rules.emplace_back(LinearRule(newLhs, rule.getRhss()[0]));
        }
        return rules;
    }

    Strengthener::Strengthener(const RuleContext &ruleCtx): ruleCtx(ruleCtx) { }

    const std::vector<GuardList> Strengthener::apply(const GuardList &guard) const {
        std::vector<GuardList> res;
        const GuardContext &guardCtx = GuardContextBuilder::build(guard, ruleCtx.update);
        const Templates &templates = TemplateBuilder::build(guardCtx, ruleCtx);
        Z3Context z3Ctx;
        const SmtConstraints &smtConstraints = ConstraintBuilder::build(templates, ruleCtx, guardCtx, z3Ctx);
        MaxSmtConstraints maxSmtConstraints = toMaxSmt(smtConstraints, guardCtx.decreasing.empty(), z3Ctx);
        if (maxSmtConstraints.hard.empty()) {
            return {};
        }
        const option<Invariants> &newInv = ConstraintSolver::solve(ruleCtx, maxSmtConstraints, templates, z3Ctx);
        if (newInv) {
            GuardList newGuard(guardCtx.guard);
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
            for (const Expression &e: newInv.get().invariants) {
                debugTest("deduced invariant " << e);
            }
            for (const Expression &e: newInv.get().pseudoInvariants) {
                GuardList pseudoInvariantInvalid(newGuard);
                assert(Relation::isInequality(e));
                const Expression &negated = Relation::negateLessEqInequality(Relation::toLessEq(e));
                debugTest("deduced pseudo-invariant " << e << ", also trying " << negated);
                pseudoInvariantInvalid.push_back(negated);
                res.emplace_back(pseudoInvariantInvalid);
            }
        }
        return res;
    }

    const MaxSmtConstraints Strengthener::toMaxSmt(const SmtConstraints &constraints, bool preferInvariance, Z3Context &z3Ctx) {
        MaxSmtConstraints res;
        res.soft.insert(res.soft.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
        if (preferInvariance) {
            z3::expr_vector allInvariant(z3Ctx);
            for (const z3::expr &e: constraints.conclusionsInvariant) {
                allInvariant.push_back(e);
                res.soft.push_back(z3::mk_and(allInvariant));
            }
        }
        z3::expr_vector sat(z3Ctx);
        for (const z3::expr &e: constraints.initiation.satisfiable) {
            sat.push_back(e);
        }
        res.hard.push_back(z3::mk_or(sat));
        z3::expr_vector someMonotonic(z3Ctx);
        for (const z3::expr &e: constraints.conclusionsMonotonic) {
            res.soft.push_back(e);
            someMonotonic.push_back(e);
        }
        res.hard.push_back(z3::mk_or(someMonotonic));
        res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
        return res;
    }

}
