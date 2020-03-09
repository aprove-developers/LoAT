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
#include "../smt/solver.hpp"
#include "../config.hpp"

namespace strengthening {

    typedef ConstraintSolver Self;

    const option<GuardList> Self::solve(
            const RuleContext &ruleCtx,
            const BoolExpr &constraints,
            const Templates &templates) {
        return ConstraintSolver(ruleCtx, constraints, templates).solve();
    }

    Self::ConstraintSolver(
            const RuleContext &ruleCtx,
            const BoolExpr &constraints,
            const Templates &templates):
            ruleCtx(ruleCtx),
            constraints(constraints),
            templates(templates) { }

    const option<GuardList> Self::solve() const {
        Solver solver(ruleCtx.varMan);
        solver.add(constraints);
        if (solver.check() == smt::Sat) {
            const GuardList &newInvariants = instantiateTemplates(solver.model());
            if (!newInvariants.empty()) {
                return {newInvariants};
            }
        }
        return {};
    }

    const GuardList Self::instantiateTemplates(const VarMap<GiNaC::numeric> &model) const {
        GuardList res;
        UpdateMap parameterInstantiation;
        for (const Var &p: templates.params()) {
            auto it = model.find(p);
            if (it != model.end()) {
                const Expr &pi = it->second;
                parameterInstantiation.emplace(ruleCtx.varMan.getVarIdx(p), pi);
            }
        }
        const ExprMap &subs = parameterInstantiation.toSubstitution(ruleCtx.varMan);
        const std::vector<Rel> instantiatedTemplates = templates.subs(subs);
        Solver solver(ruleCtx.varMan);
        for (const Rel &rel: instantiatedTemplates) {
            if (!templates.isParametric(rel)) {
                solver.push();
                solver.add(!buildLit(rel));
                if (solver.check() != smt::Unsat) {
                    res.push_back(rel);
                }
                solver.pop();
            }
        }
        return res;
    }

}
