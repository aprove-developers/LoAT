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

#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"
#include "guardcontextbuilder.hpp"

namespace strengthening {

    typedef GuardContextBuilder Self;

    const GuardContext Self::build(const GuardList &guard, const std::vector<Subs> &updates, const VariableManager &varMan) {
        return GuardContextBuilder(guard, updates, varMan).build();
    }

    Self::GuardContextBuilder(
            const GuardList &guard,
            const std::vector<Subs> &updates,
            const VariableManager &varMan
    ): guard(guard), updates(updates), varMan(varMan) { }

    const GuardList Self::computeConstraints() const {
        GuardList constraints;
        for (const Rel &rel: guard) {
            if (rel.isLinear() && rel.isEq()) {
                constraints.emplace_back(rel.lhs() <= rel.rhs());
                constraints.emplace_back(rel.rhs() <= rel.lhs());
            } else if (rel.isLinear() && rel.isIneq()) {
                constraints.push_back(rel);
            }
        }
        return constraints;
    }

    const Result Self::splitInvariants(const GuardList &constraints) const {
        std::unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic({guard}, updates), varMan);
        for (const Rel &rel: guard) {
            solver->add(rel);
        }
        Result res;
        for (const Rel &rel: constraints) {
            bool isInvariant = true;
            for (const Subs &up: updates) {
                Rel conclusionExp = rel;
                conclusionExp.applySubs(up);
                const BoolExpr &conclusion = buildLit(conclusionExp);
                solver->push();
                solver->add(!conclusion);
                const Smt::Result &smtRes = solver->check();
                solver->pop();
                if (smtRes != Smt::Unsat) {
                    isInvariant = false;
                    break;
                }
            }
            if (isInvariant) {
                res.solved.push_back(rel);
            } else {
                res.failed.push_back(rel);
            }
        }
        return res;
    }

    const GuardContext Self::build() const {
        const GuardList &constraints = computeConstraints();
        const Result inv = splitInvariants(constraints);
        return GuardContext(guard, inv.solved, inv.failed);
    }

}
