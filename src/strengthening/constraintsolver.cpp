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

#include "../util/option.hpp"
#include "constraintsolver.hpp"
#include "../its/variablemanager.hpp"
#include "../util/timeout.hpp"
#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"
#include "../config.hpp"

namespace strengthening {

    typedef ConstraintSolver Self;

    const option<Invariants> Self::solve(
            const RuleContext &ruleCtx,
            const MaxSmtConstraints &constraints,
            const Templates &templates) {
        return ConstraintSolver(ruleCtx, constraints, templates).solve();
    }

    Self::ConstraintSolver(
            const RuleContext &ruleCtx,
            const MaxSmtConstraints &constraints,
            const Templates &templates):
            ruleCtx(ruleCtx),
            constraints(constraints),
            templates(templates) { }

    const option<Invariants> Self::solve() const {
        const option<ExprSymbolMap<GiNaC::numeric>> &model = Smt::maxSmt(constraints.hard, constraints.soft, Config::Z3::StrengtheningTimeout, ruleCtx.varMan);
        if (model) {
            const GuardList &newInvariants = instantiateTemplates(model.get());
            if (!newInvariants.empty()) {
                return splitInitiallyValid(newInvariants);
            }
        }
        return {};
    }

    const GuardList Self::instantiateTemplates(const ExprSymbolMap<GiNaC::numeric> &model) const {
        GuardList res;
        UpdateMap parameterInstantiation;
        for (const ExprSymbol &p: templates.params()) {
            auto it = model.find(p);
            if (it != model.end()) {
                const Expression &pi = it->second;
                parameterInstantiation.emplace(ruleCtx.varMan.getVarIdx(p), pi);
            }
        }
        const ExprMap &subs = parameterInstantiation.toSubstitution(ruleCtx.varMan);
        const std::vector<Rel> instantiatedTemplates = templates.subs(subs);
        std::unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic<UpdateMap>({instantiatedTemplates}, {}), ruleCtx.varMan);
        for (const Rel &rel: instantiatedTemplates) {
            if (!templates.isParametric(rel)) {
                solver->push();
                solver->add(!buildLit(rel));
                if (solver->check() != Smt::Unsat) {
                    res.push_back(rel);
                }
                solver->pop();
            }
        }
        return res;
    }

    const option<Invariants> Self::splitInitiallyValid(const GuardList &invariants) const {
        Invariants res;
        BoolExpr preconditionVec = False;
        for (const GuardList &pre: ruleCtx.preconditions) {
            BoolExpr preVec = True;
            for (const Rel &rel: pre) {
                preVec = preVec & rel;
            }
            preconditionVec = preconditionVec | preVec;
        }
        std::unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic<UpdateMap>({invariants}, {}), ruleCtx.varMan);
        for (const Rel &rel: invariants) {
            solver->push();
            solver->add(!rel);
            if (solver->check() == Smt::Unsat) {
                res.invariants.push_back(rel);
            } else {
                res.pseudoInvariants.push_back(rel);
            }
            solver->pop();
        }
        return {res};
    }

}
