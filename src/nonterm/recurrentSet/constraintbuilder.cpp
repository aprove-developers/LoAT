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
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#include "../../accelerate/meter/farkas.hpp"
#include "constraintbuilder.hpp"
#include <algorithm>

namespace strengthening {

    ConstraintBuilder::ConstraintBuilder(
            const Templates &templates,
            const Rule &rule,
            const GuardContext &guardCtx,
            VariableManager &varMan
    ) : templates(templates),
        rule(rule),
        guardCtx(guardCtx),
        varMan(varMan) {}

    const BoolExpr ConstraintBuilder::buildSmtConstraints(const Templates &templates,
                                                          const Rule &rule,
                                                          const GuardContext &guardCtx,
                                                          VariableManager &varMan) {
        ConstraintBuilder builder(templates, rule, guardCtx, varMan);
        const SmtConstraints &constraints = builder.buildSmtConstraints();
        BoolExpr res = constraints.initiation;
        res = res & constraints.conclusionsInvariant;
        res = res & constraints.templatesInvariant;
        return res;
    }

    const SmtConstraints ConstraintBuilder::buildSmtConstraints() const {
        Guard monotonicityPremise;
        const RelSet &irrelevantConstraints = findIrrelevantConstraints();
        const BoolExpr reducedGuard = rule.getGuard()->removeRels(irrelevantConstraints).get();
        Implication templatesInvariantImplication = buildTemplatesInvariantImplication(reducedGuard);
        BoolExpr invariancePremise = templatesInvariantImplication.premise;
        BoolExpr conclusionInvariant = True;
        for (const Rel &rel: guardCtx.todo) {
            for (const Subs &up: rule.getUpdates()) {
                Rel updated = rel;
                updated.applySubs(up);
                const BoolExpr invariant = constructImplicationConstraints(invariancePremise, {updated});
                conclusionInvariant = conclusionInvariant & invariant;
            }
        }
        const BoolExpr initiation = constructInitiationConstraints(reducedGuard);
        const BoolExpr templatesInvariant = constructImplicationConstraints(
                templatesInvariantImplication.premise,
                templatesInvariantImplication.conclusion);
        return SmtConstraints(initiation, templatesInvariant, conclusionInvariant);
    }

    const RelSet ConstraintBuilder::findIrrelevantConstraints() const {
        RelSet irrelevantConstraints;
        const VarSet &templateVars = templates.vars();
        for (const Rel &rel: guardCtx.guard->lits()) {
            bool irrelevant = true;
            for (const Var &var: rel.vars()) {
                if (templateVars.find(var) != templateVars.end()) {
                    irrelevant = false;
                    break;
                }
            }
            if (irrelevant) {
                irrelevantConstraints.insert(rel);
            }
        }
        return irrelevantConstraints;
    }

    const Implication ConstraintBuilder::buildTemplatesInvariantImplication(const BoolExpr &reducedGuard) const {
        Implication res;
        RelSet premise;
        RelSet conclusion;
        for (const Rel &invTemplate: templates) {
            Guard updatedTemplates;
            for (const Subs &up: rule.getUpdates()) {
                Rel updated = invTemplate;
                updated.applySubs(up);
                updatedTemplates.push_back(updated);
            }
            premise.insert(invTemplate);
            for (const Rel &rel: updatedTemplates) {
                conclusion.insert(rel);
            }
        }
        return {reducedGuard & buildAnd(premise), conclusion};
    }

    const BoolExpr ConstraintBuilder::constructInitiationConstraints(const BoolExpr &reducedGuard) const {
        BoolExpr res = False;
        VarSet allVars = reducedGuard->vars();
        // TODO Why is this variable renaming needed?
        Subs varRenaming;
        for (const Var &x: allVars) {
            varRenaming.put(x, varMan.addFreshVariable(x.get_name()));
        }
        std::vector<Rel> renamed;
        const std::vector<Rel> &updatedTemplates = templates.subs(varRenaming);
        for (const Rel &e: updatedTemplates) {
            renamed.push_back(e);
        }
        const BoolExpr expr = reducedGuard->subs(varRenaming) & buildAnd(renamed);
        res = res | expr;
        return res;
    }

    const BoolExpr ConstraintBuilder::constructImplicationConstraints(
            const BoolExpr &premise,
            const RelSet &conclusion) const {
        return FarkasLemma::apply(
                premise,
                conclusion,
                templates.vars(),
                templates.params(),
                varMan,
                Expr::Int);
    }

}
