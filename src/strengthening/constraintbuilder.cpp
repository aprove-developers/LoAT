//
// Created by ffrohn on 2/8/19.
//

#include <expr/relation.h>
#include <accelerate/meter/farkas.h>
#include "constraintbuilder.h"
#include <z3/z3solver.h>
#include <z3/z3toolbox.h>

namespace strengthening {

    typedef ConstraintBuilder Self;

    Self::ConstraintBuilder(
            const Templates &templates,
            const GuardList &relevantConstraints,
            const Context &context,
            Z3Context &z3Context
    ) : templates(templates),
        relevantConstraints(relevantConstraints),
        context(context),
        z3Context(z3Context) {
    }

    const SmtConstraints Self::buildSMTConstraints() const {
        GuardList invariancePremise;
        GuardList invarianceConclusion;
        GuardList monotonicityPremise;
        GuardList monotonicityConclusion;
        invariancePremise.insert(invariancePremise.end(), relevantConstraints.begin(), relevantConstraints.end());
        for (const Expression &e: context.todo) {
            for (const GiNaC::exmap &up: context.updates) {
                Expression updated = e;
                updated.applySubs(up);
                if (Relation::isLinearInequality(updated, templates.vars())) {
                    invarianceConclusion.push_back(updated);
                }
            }
        }
        for (const Expression &e: relevantConstraints) {
            for (const GiNaC::exmap &up: context.updates) {
                Expression updated = e;
                updated.applySubs(up);
                monotonicityPremise.push_back(updated);
            }
        }
        for (const GiNaC::exmap &up: context.updates) {
            const std::vector<Expression> &updated = templates.subs(up);
            monotonicityPremise.insert(monotonicityPremise.end(), updated.begin(), updated.end());
        }
        monotonicityPremise.insert(monotonicityPremise.end(), templates.begin(), templates.end());
        monotonicityPremise.insert(monotonicityPremise.end(), context.invariants.begin(), context.invariants.end());
        for (const Expression &e: context.todo) {
            if (Relation::isLinearInequality(e, templates.vars())) {
                monotonicityConclusion.push_back(e);
            }
        }
        const Implication &templatesInvariantImplication = buildTemplatesInvariantImplication();
        // We use templatesInvariantImplication.premise instead of templates as buildTemplatesInvariantImplication
        // discards templates that become non-linear when applying the update.
        invariancePremise.insert(
                invariancePremise.end(),
                templatesInvariantImplication.premise.begin(),
                templatesInvariantImplication.premise.end());
        const Initiation &initiation = constructSmtInitiationConstraints(relevantConstraints);
        const std::vector<z3::expr> templatesInvariant = constructImplicationConstraints(
                invariancePremise,
                templatesInvariantImplication.conclusion);
        const std::vector<z3::expr> &conclusionInvariant = constructImplicationConstraints(
                invariancePremise,
                invarianceConclusion);
        const std::vector<z3::expr> &conclusionMonotonic = constructImplicationConstraints(
                monotonicityPremise,
                monotonicityConclusion);
        return SmtConstraints(initiation, templatesInvariant, conclusionInvariant, conclusionMonotonic);
    }

    const Implication Self::buildTemplatesInvariantImplication() const {
        Implication res;
        for (const Expression &invTemplate: templates) {
            bool linear = true;
            GuardList updatedTemplates;
            for (const GiNaC::exmap &up: context.updates) {
                Expression updated = invTemplate;
                updated.applySubs(up);
                if (Relation::isLinearInequality(updated, templates.vars())) {
                    updatedTemplates.push_back(updated);
                } else {
                    linear = false;
                    break;
                }
            }
            if (linear) {
                res.premise.push_back(invTemplate);
                for (const Expression &t: updatedTemplates) {
                    res.conclusion.push_back(t);
                }
            }
        }
        return res;
    }

    const Initiation Self::constructSmtInitiationConstraints(const GuardList &premise) const {
        Initiation res;
        for (const GuardList &pre: context.preconditions) {
            z3::expr_vector gVec(z3Context);
            for (const Expression &e: pre) {
                gVec.push_back(e.toZ3(z3Context));
            }
            for (const Expression &t: templates) {
                res.valid.push_back(constructImplicationConstraints(pre, t));
            }
            ExprSymbolSet allVars;
            pre.collectVariables(allVars);
            for (const Expression &e: premise) {
                e.collectVariables(allVars);
            }
            for (const Expression &e: pre) {
                e.collectVariables(allVars);
            }
            GiNaC::exmap varRenaming;
            for (const ExprSymbol &x: allVars) {
                varRenaming[x] = context.varMan.getVarSymbol(context.varMan.addFreshVariable(x.get_name()));
            }
            z3::expr_vector renamed(z3Context);
            for (Expression e: pre) {
                e.applySubs(varRenaming);
                renamed.push_back(e.toZ3(z3Context));
            }
            const std::vector<Expression> &updatedTemplates = templates.subs(varRenaming);
            for (const Expression &e: updatedTemplates) {
                renamed.push_back(e.toZ3(z3Context));
            }
            for (Expression e: premise) {
                e.applySubs(varRenaming);
                renamed.push_back(e.toZ3(z3Context));
            }
            const z3::expr &expr = mk_and(renamed);
            res.satisfiable.push_back(expr);
        }
        return res;
    }

    const std::vector<z3::expr> Self::constructImplicationConstraints(
            const GuardList &premise,
            const GuardList &conclusion) const {
        return FarkasLemma::apply(
                premise,
                conclusion,
                templates.vars(),
                templates.params(),
                z3Context,
                Z3Context::Integer);
    }

    const z3::expr Self::constructImplicationConstraints(
            const GuardList &premise,
            const Expression &conclusion) const {
        return FarkasLemma::apply(
                premise,
                conclusion,
                templates.vars(),
                templates.params(),
                z3Context,
                Z3Context::Integer);
    }

}