//
// Created by ffrohn on 2/18/19.
//

#include "../expr/relation.hpp"
#include "../z3/z3context.hpp"
#include "../z3/z3solver.hpp"
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

}
