//
// Created by ffrohn on 2/18/19.
//

#include <expr/relation.h>
#include <z3/z3context.h>
#include <z3/z3solver.h>
#include "strengthener.h"
#include "templatebuilder.h"
#include "constraintsolver.h"
#include "setup.h"

namespace strengthening {

    typedef Strengthener Self;

    const std::vector<Rule> Self::apply(const Rule &rule, ITSProblem &its) {
        std::stack<Rule> todo;
        todo.push(rule);
        std::vector<Rule> res;
        while (!todo.empty()) {
            const Rule &current = todo.top();
            if (current.isSimpleLoop()) {
                const Context &context = Setup(current, its).setup();
                const Strengthener &strengthener = Strengthener(context);
                std::vector<Rule> strong(strengthener.apply(Modes::invariance));
                if (strong.empty()) {
                    strong = strengthener.apply(Modes::pseudoInvariance);
                    if (strong.empty() && current.getGuard() != rule.getGuard()) {
                        res.push_back(current);
                    }
                }
                todo.pop();
                for (const Rule &s: strong) {
                    todo.push(s);
                }
            }
        }
        return res;
    }

    Self::Strengthener(const Context &context): context(context) {}

    const std::vector<Rule> Self::apply(const Mode &mode) const {
        std::vector<Rule> res;
        const Templates &templates = TemplateBuilder(context.todo, context.rule, context.varMan).buildTemplates();
        Z3Context z3Context;
        const SmtConstraints &smtConstraints = buildSmtConstraints(
                templates,
                z3Context);
        MaxSmtConstraints maxSmtConstraints = mode(smtConstraints, z3Context);
        if (maxSmtConstraints.hard.empty()) {
            return {};
        }
        ConstraintSolver solver(context.preconditions, maxSmtConstraints, templates, z3Context, context.varMan);
        const option<Invariants> &newInv = solver.solve();
        if (newInv) {
            GuardList newGuard(context.rule.getGuard());
            newGuard.insert(
                    newGuard.end(),
                    newInv.get().invariants.begin(),
                    newInv.get().invariants.end());
            GuardList pseudoInvariantsValid(newGuard);
            pseudoInvariantsValid.insert(
                    pseudoInvariantsValid.end(),
                    newInv.get().pseudoInvariants.begin(),
                    newInv.get().pseudoInvariants.end());
            const RuleLhs invLhs(context.rule.getLhsLoc(), pseudoInvariantsValid, context.rule.getCost());
            res.emplace_back(Rule(invLhs, context.rule.getRhss()));
            for (const Expression &e: newInv.get().pseudoInvariants) {
                GuardList pseudoInvariantInvalid(newGuard);
                assert(Relation::isInequality(e));
                const Expression &negated = Relation::negateLessEqInequality(Relation::toLessEq(e));
                debugTest("deduced pseudo-invariant " << e << ", also trying " << negated);
                pseudoInvariantInvalid.push_back(negated);
                RuleLhs pseudoInvLhs(context.rule.getLhsLoc(), pseudoInvariantInvalid, context.rule.getCost());
                res.emplace_back(Rule(pseudoInvLhs, context.rule.getRhss()));
            }
        }
        return res;
    }

    const SmtConstraints Self::buildSmtConstraints(
            const Templates &templates,
            Z3Context &z3Context) const {

        const GuardList &relevantConstraints = findRelevantConstraints(context.rule.getGuard(), templates.vars());
        return ConstraintBuilder(
                templates,
                relevantConstraints,
                context,
                z3Context).buildSMTConstraints();
    }

    const GuardList Self::findRelevantConstraints(const GuardList &guard, const ExprSymbolSet &vars) const {
        GuardList relevantConstraints;
        for (const Expression &e: guard) {
            for (const ExprSymbol &var: vars) {
                if (e.getVariables().count(var) > 0) {
                    relevantConstraints.push_back(e);
                    break;
                }
            }
        }
        return relevantConstraints;
    }

}