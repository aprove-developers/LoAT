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

namespace strengthening {

    ConstraintBuilder::ConstraintBuilder(
            const Templates &templates,
            const RuleContext &ruleCtx,
            const GuardContext &guardCtx
    ) : templates(templates),
        ruleCtx(ruleCtx),
        guardCtx(guardCtx) {
    }

    const MaxSmtConstraints ConstraintBuilder::buildMaxSmtConstraints(const Templates &templates,
                                                                      const RuleContext &ruleCtx,
                                                                      const GuardContext &guardCtx) {
        ConstraintBuilder builder(templates, ruleCtx, guardCtx);
        const SmtConstraints &constraints = builder.buildSmtConstraints();
        MaxSmtConstraints res;
        res.soft.push_back(constraints.initiation.valid);
        BoolExpr sat = False;
        for (const BoolExpr &e: constraints.initiation.satisfiable) {
            sat = sat | e;
        }
        res.hard = res.hard & sat;
        res.hard = res.hard & constraints.conclusionsInvariant;
        res.hard = res.hard & constraints.templatesInvariant;
        return res;
    }

    const SmtConstraints ConstraintBuilder::buildSmtConstraints() const {
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
        BoolExpr conclusionInvariant = True;
        for (const Rel &rel: guardCtx.todo) {
            for (const ExprMap &up: ruleCtx.updates) {
                Rel updated = rel;
                updated.applySubs(up);
                const BoolExpr &invariant = constructImplicationConstraints(invariancePremise, updated);
                conclusionInvariant = conclusionInvariant & invariant;
            }
        }
        const Initiation &initiation = constructInitiationConstraints(relevantConstraints);
        const BoolExpr &templatesInvariant = constructImplicationConstraints(
                templatesInvariantImplication.premise,
                templatesInvariantImplication.conclusion);
        return SmtConstraints(initiation, templatesInvariant, conclusionInvariant);
    }

    const GuardList ConstraintBuilder::findRelevantConstraints() const {
        GuardList relevantConstraints;
        for (const Rel &rel: guardCtx.guard) {
            for (const ExprSymbol &var: templates.vars()) {
                if (rel.getVariables().count(var) > 0) {
                    relevantConstraints.push_back(rel);
                    break;
                }
            }
        }
        return relevantConstraints;
    }

    const Implication ConstraintBuilder::buildTemplatesInvariantImplication() const {
        Implication res;
        for (const Rel &invTemplate: templates) {
            GuardList updatedTemplates;
            for (const ExprMap &up: ruleCtx.updates) {
                Rel updated = invTemplate;
                updated.applySubs(up);
                updatedTemplates.push_back(updated);
            }
            res.premise.push_back(invTemplate);
            for (const Rel &rel: updatedTemplates) {
                res.conclusion.push_back(rel);
            }
        }
        for (const Rel &rel: guardCtx.guard) {
            res.premise.push_back(rel);
        }
        return res;
    }

    const Initiation ConstraintBuilder::constructInitiationConstraints(const GuardList &relevantConstraints) const {
        Initiation res;
        res.valid = True;
        for (const GuardList &pre: ruleCtx.preconditions) {
            for (const Rel &t: templates) {
                res.valid = res.valid & constructImplicationConstraints(pre, t);
            }
            ExprSymbolSet allVars;
            pre.collectVariables(allVars);
            for (const Rel &rel: relevantConstraints) {
                rel.collectVariables(allVars);
            }
            for (const Rel &rel: pre) {
                rel.collectVariables(allVars);
            }
            // TODO Why is this variable renaming needed?
            ExprMap varRenaming;
            for (const ExprSymbol &x: allVars) {
                varRenaming[x] = ruleCtx.varMan.getVarSymbol(ruleCtx.varMan.addFreshVariable(x.get_name()));
            }
            std::vector<Rel> renamed;
            for (Rel rel: pre) {
                rel.applySubs(varRenaming);
                renamed.push_back(rel);
            }
            const std::vector<Rel> &updatedTemplates = templates.subs(varRenaming);
            for (const Rel &e: updatedTemplates) {
                renamed.push_back(e);
            }
            for (Rel rel: relevantConstraints) {
                rel.applySubs(varRenaming);
                renamed.push_back(rel);
            }
            const BoolExpr &expr = buildAnd(renamed);
            res.satisfiable.push_back(expr);
        }
        return res;
    }

    const BoolExpr ConstraintBuilder::constructImplicationConstraints(
            const GuardList &premise,
            const GuardList &conclusion) const {
        return FarkasLemma::apply(
                premise,
                conclusion,
                templates.vars(),
                templates.params(),
                ruleCtx.varMan,
                Expression::Int);
    }

    const BoolExpr ConstraintBuilder::constructImplicationConstraints(
            const GuardList &premise,
            const Rel &conclusion) const {
        return FarkasLemma::apply(
                premise,
                conclusion,
                templates.vars(),
                templates.params(),
                ruleCtx.varMan,
                Expression::Int);
    }

}
