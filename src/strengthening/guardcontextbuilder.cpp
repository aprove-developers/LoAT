//
// Created by ffrohn on 2/25/19.
//

#include "../expr/relation.hpp"
#include "../z3/z3solver.hpp"
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
        for (const GiNaC::exmap &up: updates) {
            solver.push();
            for (Expression g: guard) {
                g.applySubs(up);
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
            solver.pop();
        }
        return res;
    }

    const GuardContext Self::build() const {
        const GuardList &constraints = computeConstraints();
        const Result inv = splitInvariants(constraints);
        const Result mon = splitMonotonicConstraints(inv.solved, inv.failed);
        return GuardContext(guard, mon.solved, mon.failed);
    }

}
