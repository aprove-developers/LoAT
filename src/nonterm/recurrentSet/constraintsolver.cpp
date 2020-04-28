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

#include "../../util/option.hpp"
#include "constraintsolver.hpp"
#include "../../its/variablemanager.hpp"
#include "../../smt/smt.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../config.hpp"

namespace strengthening {

    typedef ConstraintSolver Self;

    const option<Guard> Self::solve(
            const BoolExpr &constraints,
            const Templates &templates,
            VariableManager &varMan) {
        return ConstraintSolver(constraints, templates, varMan).solve();
    }

    Self::ConstraintSolver(
            const BoolExpr &constraints,
            const Templates &templates,
            VariableManager &varMan):
            constraints(constraints),
            templates(templates),
            varMan(varMan) { }

    const option<Guard> Self::solve() const {
        std::unique_ptr<Smt> solver = SmtFactory::modelBuildingSolver(Smt::chooseLogic({constraints}), varMan);
        solver->add(constraints);
        if (solver->check() == Smt::Sat) {
            const Guard &newInvariants = instantiateTemplates(solver->model());
            if (!newInvariants.empty()) {
                return {newInvariants};
            }
        }
        return {};
    }

    const Guard Self::instantiateTemplates(const Model &model) const {
        Guard res;
        Subs parameterInstantiation;
        for (const Var &p: templates.params()) {
            if (model.contains(p)) {
                const Expr &pi = model.get(p);
                parameterInstantiation.put(p, pi);
            }
        }
        const std::vector<Rel> instantiatedTemplates = templates.subs(parameterInstantiation);
        for (const Rel &rel: instantiatedTemplates) {
            if (!templates.isParametric(rel)) {
                res.push_back(rel);
            }
        }
        return res;
    }

}
