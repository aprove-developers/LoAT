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
#include "../accelerate/meter/farkas.hpp"
#include "constraintbuilder.hpp"
#include "../z3/z3toolbox.hpp"

namespace strengthening {


    const SmtConstraints ConstraintBuilder::build(
            const Templates &templates,
            const RuleContext &ruleCtx,
            const GuardContext &guardCtx,
            Z3Context &z3Ctx) {
        return ConstraintBuilder(templates, ruleCtx, guardCtx, z3Ctx).build();
    }

    ConstraintBuilder::ConstraintBuilder(
            const Templates &templates,
            const RuleContext &ruleCtx,
            const GuardContext &guardCtx,
            Z3Context &z3Ctx
    ) : templates(templates),
        ruleCtx(ruleCtx),
        guardCtx(guardCtx),
        z3Ctx(z3Ctx) {
    }

    const SmtConstraints ConstraintBuilder::build() const {
        GuardList invariancePremise;
        GuardList monotonicityPremise;
        const GuardList &relevantConstraints = findRelevantConstraints();
        invariancePremise.insert(invariancePremise.end(), relevantConstraints.begin(), relevantConstraints.end());
        Implication templatesInvariantImplication = buildTemplatesInvariantImplication();
        // We use templatesInvariantImplication.premise instead of templates as buildTemplatesInvariantImplication
        // discards templates that become non-linear when applying the update.
        invariancePremise.insert(
                invariancePremise.end(),
                templatesInvariantImplication.premise.begin(),
                templatesInvariantImplication.premise.end());
        templatesInvariantImplication.premise.insert(
                templatesInvariantImplication.premise.end(),
                guardCtx.invariants.begin(),
                guardCtx.invariants.end());
        std::vector<z3::expr> conclusionInvariant;
        for (const Expression &e: guardCtx.todo) {
            for (const GiNaC::exmap &up: ruleCtx.updates) {
                Expression updated = e;
                updated.applySubs(up);
                if (Relation::isLinearInequality(e, templates.vars()) && Relation::isLinearInequality(updated, templates.vars())) {
                    const z3::expr &invariant = constructImplicationConstraints(invariancePremise, updated);
                    conclusionInvariant.push_back(invariant);
                }
            }
        }
        const Initiation &initiation = constructInitiationConstraints(relevantConstraints);
        const std::vector<z3::expr> templatesInvariant = constructImplicationConstraints(
                templatesInvariantImplication.premise,
                templatesInvariantImplication.conclusion);
        return SmtConstraints(initiation, templatesInvariant, conclusionInvariant);
    }

    const GuardList ConstraintBuilder::findRelevantConstraints() const {
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

    const Implication ConstraintBuilder::buildTemplatesInvariantImplication() const {
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

    const Initiation ConstraintBuilder::constructInitiationConstraints(const GuardList &relevantConstraints) const {
        Initiation res;
        for (const GuardList &pre: ruleCtx.preconditions) {
            for (const Expression &t: templates) {
                res.valid.push_back(constructImplicationConstraints(pre, t));
            }
            ExprSymbolSet allVars;
            pre.collectVariables(allVars);
            for (const Expression &e: relevantConstraints) {
                e.collectVariables(allVars);
            }
            for (const Expression &e: pre) {
                e.collectVariables(allVars);
            }
            // TODO Why is this variable renaming needed?
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
            for (Expression e: relevantConstraints) {
                e.applySubs(varRenaming);
                renamed.push_back(e.toZ3(z3Ctx));
            }
            const z3::expr &expr = mk_and(renamed);
            res.satisfiable.push_back(expr);
        }
        return res;
    }

    const std::vector<z3::expr> ConstraintBuilder::constructImplicationConstraints(
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

    const z3::expr ConstraintBuilder::constructImplicationConstraints(
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
