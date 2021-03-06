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

#include "../../smt/smt.hpp"
#include "../../smt/smtfactory.hpp"
#include "guardcontextbuilder.hpp"

namespace strengthening {

    typedef GuardContextBuilder Self;

    const GuardContext Self::build(const BoolExpr guard, const std::vector<Subs> &updates, VariableManager &varMan) {
        return GuardContextBuilder(guard, updates, varMan).build();
    }

    Self::GuardContextBuilder(
            const BoolExpr guard,
            const std::vector<Subs> &updates,
            VariableManager &varMan
    ): guard(guard), updates(updates), varMan(varMan) { }

    const Guard Self::computeConstraints() const {
        Guard constraints;
        for (const Rel &rel: guard->lits()) {
            if (rel.isLinear() && rel.isEq()) {
                constraints.emplace_back(rel.lhs() <= rel.rhs());
                constraints.emplace_back(rel.rhs() <= rel.lhs());
            } else if (rel.isLinear() && rel.isIneq()) {
                constraints.push_back(rel);
            }
        }
        return constraints;
    }

    const GuardContextBuilder::Split Self::splitInvariants(const Guard &constraints) const {
        std::unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic({guard}, updates), varMan);
        solver->add(guard);
        GuardContextBuilder::Split res;
        for (const Rel &rel: constraints) {
            solver->push();
            solver->add(rel);
            bool isInvariant = true;
            for (const Subs &up: updates) {
                Rel conclusionExp = rel;
                conclusionExp.applySubs(up);
                const BoolExpr conclusion = buildLit(conclusionExp);
                solver->push();
                solver->add(!conclusion);
                const Smt::Result &smtRes = solver->check();
                solver->pop();
                if (smtRes != Smt::Unsat) {
                    isInvariant = false;
                    break;
                }
            }
            solver->pop();
            if (isInvariant) {
                res.invariant.push_back(rel);
            } else {
                res.nonInvariant.push_back(rel);
            }
        }
        return res;
    }

    const GuardContext Self::build() const {
        const Guard &constraints = computeConstraints();
        const GuardContextBuilder::Split inv = splitInvariants(constraints);
        return GuardContext(guard, inv.nonInvariant);
    }

}
