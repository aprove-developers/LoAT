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
#include "../z3/z3solver.hpp"
#include "../z3/z3toolbox.hpp"
#include "guardcontextbuilder.hpp"

namespace strengthening {

    typedef GuardContextBuilder Self;

    const GuardContext Self::build(const GuardList &guard, const std::vector<GiNaC::exmap> &updates) {
        return GuardContextBuilder(guard, updates).build();
    }

    Self::GuardContextBuilder(
            const GuardList &guard,
            const std::vector<GiNaC::exmap> &updates
    ): guard(guard), updates(updates) { }

    const GuardList Self::computeConstraints() const {
        GuardList constraints;
        for (const Expression &e: guard) {
            if (Relation::isLinearEquality(e)) {
                constraints.emplace_back(e.lhs() <= e.rhs());
                constraints.emplace_back(e.rhs() <= e.lhs());
            } else if (Relation::isLinearInequality(e)) {
                constraints.push_back(e);
            }
        }
        return constraints;
    }

    const Result Self::splitInvariants(const GuardList &constraints) const {
        Z3Context z3Ctx;
        Z3Solver solver(z3Ctx);
        for (const Expression &g: guard) {
            solver.add(g.toZ3(z3Ctx));
        }
        Result res;
        for (const Expression &g: constraints) {
            bool isInvariant = true;
            for (const GiNaC::exmap &up: updates) {
                Expression conclusionExp = g;
                conclusionExp.applySubs(up);
                const z3::expr &conclusion = conclusionExp.toZ3(z3Ctx);
                solver.push();
                solver.add(!conclusion);
                const z3::check_result &z3Res = solver.check();
                solver.pop();
                if (z3Res != z3::check_result::unsat) {
                    isInvariant = false;
                    break;
                }
            }
            if (isInvariant) {
                res.solved.push_back(g);
            } else {
                res.failed.push_back(g);
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
