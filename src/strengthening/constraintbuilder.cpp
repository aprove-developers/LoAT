//
// Created by ffrohn on 2/8/19.
//

#include <expr/relation.h>
#include <accelerate/meter/farkas.h>
#include "constraintbuilder.h"
#include <z3/z3toolbox.h>

namespace strengthening {

    typedef ConstraintBuilder Self;

    const SmtConstraints Self::build(
            const Templates &templates,
            const RuleContext &ruleCtx,
            const GuardContext &guardCtx,
            Z3Context &z3Ctx) {
        return ConstraintBuilder(templates, ruleCtx, guardCtx, z3Ctx).build();
    }

    Self::ConstraintBuilder(
            const Templates &templates,
            const RuleContext &ruleCtx,
            const GuardContext &guardCtx,
            Z3Context &z3Ctx
    ) : templates(templates),
        ruleCtx(ruleCtx),
        guardCtx(guardCtx),
        z3Ctx(z3Ctx) {
    }

    const SmtConstraints Self::build() const {
        GuardList invariancePremise;
        GuardList invarianceConclusion;
        GuardList monotonicityPremise;
        GuardList monotonicityConclusion;
        const GuardList &relevantConstraints = findRelevantConstraints();
        invariancePremise.insert(invariancePremise.end(), relevantConstraints.begin(), relevantConstraints.end());
        for (const Expression &e: relevantConstraints) {
            for (const GiNaC::exmap &up: ruleCtx.updates) {
                Expression updated = e;
                updated.applySubs(up);
                monotonicityPremise.push_back(updated);
            }
        }
        monotonicityPremise.insert(monotonicityPremise.end(), guardCtx.invariants.begin(), guardCtx.invariants.end());
        const Implication &templatesInvariantImplication = buildTemplatesInvariantImplication();
        // We use templatesInvariantImplication.premise instead of templates as buildTemplatesInvariantImplication
        // discards templates that become non-linear when applying the update.
        invariancePremise.insert(
                invariancePremise.end(),
                templatesInvariantImplication.premise.begin(),
                templatesInvariantImplication.premise.end());
        monotonicityPremise.insert(
                monotonicityPremise.end(),
                templatesInvariantImplication.premise.begin(),
                templatesInvariantImplication.premise.end());
        monotonicityPremise.insert(
                monotonicityPremise.end(),
                templatesInvariantImplication.conclusion.begin(),
                templatesInvariantImplication.conclusion.end());
        for (const Expression &e: guardCtx.todo) {
            for (const GiNaC::exmap &up: ruleCtx.updates) {
                Expression updated = e;
                updated.applySubs(up);
                if (Relation::isLinearInequality(updated, templates.vars())) {
                    invarianceConclusion.push_back(updated);
                }
            }
            if (Relation::isLinearInequality(e, templates.vars())) {
                monotonicityConclusion.push_back(e);
            }
        }
        const Initiation &initiation = constructInitiationConstraints(relevantConstraints);
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

    const GuardList Self::findRelevantConstraints() const {
        GuardList relevantConstraints;
        for (const Expression &e: guardCtx.guard) {
            for (const ExprSymbol &var: templates.vars()) {
                if (e.getVariables().count(var) > 0) {
                    relevantConstraints.push_back(e);
                    break;
                }
            }
        }
        return relevantConstraints;
    }

    const Implication Self::buildTemplatesInvariantImplication() const {
        Implication res;
        for (const Expression &invTemplate: templates) {
            bool linear = true;
            GuardList updatedTemplates;
            for (const GiNaC::exmap &up: ruleCtx.updates) {
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

    const Initiation Self::constructInitiationConstraints(const GuardList &premise) const {
        Initiation res;
        for (const GuardList &pre: ruleCtx.preconditions) {
            z3::expr_vector gVec(z3Ctx);
            for (const Expression &e: pre) {
                gVec.push_back(e.toZ3(z3Ctx));
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
                varRenaming[x] = ruleCtx.varMan.getVarSymbol(ruleCtx.varMan.addFreshVariable(x.get_name()));
            }
            z3::expr_vector renamed(z3Ctx);
            for (Expression e: pre) {
                e.applySubs(varRenaming);
                renamed.push_back(e.toZ3(z3Ctx));
            }
            const std::vector<Expression> &updatedTemplates = templates.subs(varRenaming);
            for (const Expression &e: updatedTemplates) {
                renamed.push_back(e.toZ3(z3Ctx));
            }
            for (Expression e: premise) {
                e.applySubs(varRenaming);
                renamed.push_back(e.toZ3(z3Ctx));
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
                z3Ctx,
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
                z3Ctx,
                Z3Context::Integer);
    }

}