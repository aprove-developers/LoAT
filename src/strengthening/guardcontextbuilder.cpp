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

    const GuardContext Self::build(const GuardList &guard, const GiNaC::exmap &update) {
        return GuardContextBuilder(guard, update).build();
    }

    Self::GuardContextBuilder(
            const GuardList &guard,
            const GiNaC::exmap &update
    ): guard(guard), update(update) { }

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
            Expression conclusionExp = g;
            conclusionExp.applySubs(update);
            const z3::expr &conclusion = conclusionExp.toZ3(z3Ctx);
            solver.push();
            solver.add(!conclusion);
            const z3::check_result &z3Res = solver.check();
            solver.pop();
            if (z3Res == z3::check_result::unsat) {
                res.solved.push_back(g);
            } else {
                res.failed.push_back(g);
            }
        }
        return res;
    }

    const Result Self::splitMonotonicConstraints(
            const GuardList &invariants,
            const GuardList &nonInvariants) const {
        Z3Context z3Ctx;
        Z3Solver solver(z3Ctx);
        for (const Expression &g: invariants) {
            solver.add(g.toZ3(z3Ctx));
        }
        Result res;
        res.solved.insert(res.solved.end(), nonInvariants.begin(), nonInvariants.end());
        for (Expression g: guard) {
            g.applySubs(update);
            solver.add(g.toZ3(z3Ctx));
        }
        auto g = res.solved.begin();
        while (g != res.solved.end()) {
            solver.push();
            solver.add(!(*g).toZ3(z3Ctx));
            const z3::check_result &z3Res = solver.check();
            solver.pop();
            if (z3Res == z3::check_result::unsat) {
                g++;
            } else {
                res.failed.push_back(*g);
                g = res.solved.erase(g);
            }
        }
        return res;
    }

    const Result Self::splitSimpleInvariants(const GuardList &invariants) const {
        Result res;
        res.solved.insert(res.solved.end(), invariants.begin(), invariants.end());
        bool done;
        do {
            done = true;
            auto it = res.solved.begin();
            while (it != res.solved.end()) {
                if (Z3Toolbox::isValidImplication(res.solved, {(*it).subs(update)})) {
                    it++;
                } else {
                    res.solved.erase(it);
                    res.failed.push_back(*it);
                    done = false;
                    break;
                }
            }
        } while (!done);
        return res;
    }

    const GuardContext Self::build() const {
        const GuardList &constraints = computeConstraints();
        const Result inv = splitInvariants(constraints);
        const Result simpleInv = splitSimpleInvariants(inv.solved);
        const Result mon = splitMonotonicConstraints(simpleInv.solved, inv.failed);
        return GuardContext(guard, inv.solved, simpleInv.solved, mon.solved, mon.failed);
    }

}
