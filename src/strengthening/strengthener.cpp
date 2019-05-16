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
#include "../z3/z3toolbox.hpp"
#include "strengthener.hpp"
#include "templatebuilder.hpp"
#include "constraintsolver.hpp"
#include "rulecontextbuilder.hpp"
#include "guardcontextbuilder.hpp"

namespace strengthening {


    const std::vector<Rule> Strengthener::apply(const Rule &rule, ITSProblem &its, const std::vector<Mode> &modes) {
        std::stack<GuardList> todo;
        todo.push(rule.getGuard());
        std::vector<GuardList> res;
        const RuleContext &ruleCtx = RuleContextBuilder::build(rule, its);
        const Strengthener strengthener(ruleCtx);
        unsigned int i = 0;
        while (!todo.empty() && i < 10) {
            i++;
            const GuardList &current = todo.top();
            bool failed = true;
            for (const Mode &mode: modes) {
                if (Timeout::soft()) {
                    return {};
                }
                const std::vector<GuardList> &strengthened = strengthener.apply(mode, current);
                if (!strengthened.empty()) {
                    failed = false;
                    todo.pop();
                    for (const GuardList &s: strengthened) {
                        todo.push(s);
                    }
                    break;
                }
            }
            if (failed) {
                if (current != rule.getGuard()) {
                    res.push_back(current);
                }
                todo.pop();
            }
        }
        std::vector<Rule> rules;
        for (const GuardList &g: res) {
            const RuleLhs newLhs(rule.getLhsLoc(), g, rule.getCost());
            rules.emplace_back(Rule(newLhs, rule.getRhss()));
        }
        return rules;
    }

    Strengthener::Strengthener(const RuleContext &ruleCtx): ruleCtx(ruleCtx) { }

    const std::vector<GuardList> Strengthener::apply(const Mode &mode, const GuardList &guard) const {
        std::vector<GuardList> res;
        const GuardContext &guardCtx = GuardContextBuilder::build(guard, ruleCtx.updates);
        const Templates &templates = TemplateBuilder::build(guardCtx, ruleCtx);
        Z3Context z3Ctx;
        const SmtConstraints &smtConstraints = ConstraintBuilder::build(templates, ruleCtx, guardCtx, z3Ctx);
        MaxSmtConstraints maxSmtConstraints = mode(smtConstraints, guardCtx.decreasing.empty(), z3Ctx);
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
            std::vector<GuardList> monotonicityPremises;
            // TODO we could also add constraints that became monotonically decreasing due to the new local invariants
            // (but not those that became decreasing due to other new invariants)
            for (const GiNaC::exmap &up: ruleCtx.updates) {
                GuardList decreasingPremise = guardCtx.decreasing.subs(up);
                GuardList g(guardCtx.simpleInvariants);
                g.insert(g.end(), newInv.get().invariants.begin(), newInv.get().invariants.end());
                g.insert(g.end(), decreasingPremise.begin(), decreasingPremise.end());
                monotonicityPremises.push_back(g);
            }
            for (const Expression &e: newInv.get().pseudoInvariants) {
                assert(Relation::isInequality(e));
                const Expression &neg = Relation::negateLessEqInequality(Relation::toLessEq(e));
                for (const Expression &t: guardCtx.todo) {
                    bool success = true;
                    for (unsigned int i = 0; i < ruleCtx.updates.size(); i++) {
                        const GiNaC::exmap &up = ruleCtx.updates[i];
                        monotonicityPremises[i].push_back(neg.subs(up));
                        monotonicityPremises[i].push_back(t.subs(up));
                        bool res = Z3Toolbox::isValidImplication(monotonicityPremises[i], {t});
                        monotonicityPremises[i].pop_back();
                        monotonicityPremises[i].pop_back();
                        newGuard.push_back(neg);
                        res = res || Z3Toolbox::isValidImplication(newGuard, {t.subs(up)});
                        newGuard.pop_back();
                        if (!res) {
                            success = false;
                            break;
                        }
                    }
                    if (success) {
                        GuardList pseudoInvariantInvalid(newGuard);
                        debugTest("deduced non-local invariant " << e << ", also trying " << neg);
                        pseudoInvariantInvalid.push_back(neg);
                        res.emplace_back(pseudoInvariantInvalid);
                        break;
                    }
                }
            }
        }
        return res;
    }

}
