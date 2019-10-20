#include <ginac/ex.h>
#include "../its/types.hpp"
#include "../its/rule.hpp"
#include "../util/option.hpp"
#include "../expr/relation.hpp"
#include "../its/variablemanager.hpp"
#include "../accelerate/recurrence/recurrence.hpp"
#include "../z3/z3context.hpp"
#include "../z3/z3solver.hpp"
#include "../expr/ginactoz3.hpp"
#include "../analysis/preprocess.hpp"

struct AccelerationProblem {
    GuardList res;
    GuardList done;
    GuardList todo;
    GiNaC::exmap up;
    GiNaC::exmap closed;
    ExprSymbol n;
    bool equivalent = true;

    AccelerationProblem(
            const GuardList &res,
            const GuardList &done,
            const GuardList &todo,
            const GiNaC::exmap &up,
            const GiNaC::exmap &closed,
            const ExprSymbol &n): res(res), done(done), todo(todo), up(up), closed(closed), n(n) {
        this->res.push_back(n > 0);
    }

    static AccelerationProblem init(const LinearRule &r, const VariableManager &varMan, const UpdateMap &closed, const ExprSymbol &n) {
        const GiNaC::exmap &up = r.getUpdate().toSubstitution(varMan);
        AccelerationProblem res({}, {}, normalize(r.getGuard()), up, closed.toSubstitution(varMan), n);
        return res;
    }

    static GuardList normalize(const GuardList &g) {
        GuardList res;
        for (const Expression &e: g) {
            if (Relation::isEquality(e)) {
                res.push_back(Relation::normalizeInequality(e.lhs() >= e.rhs()));
                res.push_back(Relation::normalizeInequality(e.lhs() <= e.rhs()));
            } else {
                res.push_back(Relation::normalizeInequality(e));
            }
        }
        return res;
    }

    bool monotonicity() {
        Z3Context ctx;
        Z3Solver solver(ctx);
        for (const Expression &e: done) {
            solver.add(e.toZ3(ctx));
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver.push();
            solver.add(GinacToZ3::convert(e.subs(up), ctx));
            if (solver.check() != z3::sat) {
                return false;
            }
            solver.add(GinacToZ3::convert(e.lhs() <= 0, ctx));
            if (solver.check() == z3::check_result::unsat) {
                proofout << std::endl << "handled " << e.toString() << " via conditional one-way monotonicity" << std::endl;
                done.push_back(e);
                res.push_back(e.subs(closed).subs({{n, n-1}}));
                todo.erase(it);
                print();
                return true;
            }
            solver.pop();
        }
        return false;
    }

    bool recurrence() {
        Z3Context ctx;
        Z3Solver solver(ctx);
        for (const Expression &e: done) {
            solver.add(e.toZ3(ctx));
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver.push();
            solver.add(GinacToZ3::convert(e, ctx));
            if (solver.check() != z3::sat) {
                return false;
            }
            solver.add(GinacToZ3::convert(e.subs(up).lhs() <= 0, ctx));
            if (solver.check() == z3::check_result::unsat) {
                proofout << std::endl << "handled " << e << " via conditional recurrent sets" << std::endl;
                done.push_back(e);
                res.push_back(e);
                todo.erase(it);
                print();
                return true;
            }
            solver.pop();
        }
        return false;
    }

    bool eventualStrictDecrease() {
        Z3Context ctx;
        Z3Solver solver(ctx);
        for (const Expression &e: done) {
            solver.add(e.toZ3(ctx));
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver.push();
            const GiNaC::ex &updated = e.lhs().subs(up);
            solver.add(GinacToZ3::convert(e.lhs() > updated, ctx));
            if (solver.check() != z3::sat) {
                return false;
            }
            solver.add(GinacToZ3::convert(updated <= updated.subs(up), ctx));
            if (solver.check() == z3::check_result::unsat) {
                proofout << std::endl << "handled " << e << " via eventual strict decrease" << std::endl;
                done.push_back(e);
                res.push_back(e);
                res.push_back(e.subs(closed).subs({{n, n-1}}));
                todo.erase(it);
                print();
                return true;
            }
            solver.pop();
        }
        return false;
    }

    bool eventualWeakDecrease() {
        Z3Context ctx;
        Z3Solver solver(ctx);
        for (const Expression &e: done) {
            solver.add(e.toZ3(ctx));
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver.push();
            const GiNaC::ex &updated = e.lhs().subs(up);
            solver.add(GinacToZ3::convert(e.lhs() >= updated, ctx));
            if (solver.check() != z3::sat) {
                return false;
            }
            solver.add(GinacToZ3::convert(updated < updated.subs(up), ctx));
            if (solver.check() == z3::check_result::unsat) {
                proofout << std::endl << "handled " << e << " via eventual weak decrease" << std::endl;
                done.push_back(e);
                res.push_back(e);
                res.push_back(e.subs(closed).subs({{n, n-1}}));
                todo.erase(it);
                print();
                return true;
            }
            solver.pop();
        }
        return false;
    }

    bool solved() {
        return todo.empty();
    }

    void simplify() {
        while (true) {
            if (recurrence() || monotonicity()) {
                continue;
            } else if (Config::Accel::UseEvDec && eventualWeakDecrease()) {
                continue;
            } else if (Config::Accel::UseEvInc && eventualWeakIncrease()) {
                continue;
            }
            break;
        }
    }

    bool eventualStrictIncrease() {
        Z3Context ctx;
        Z3Solver solver(ctx);
        for (const Expression &e: done) {
            solver.add(e.toZ3(ctx));
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver.push();
            const GiNaC::ex &updated = e.lhs().subs(up);
            solver.add(GinacToZ3::convert(e.lhs() < updated, ctx));
            if (solver.check() != z3::sat) {
                return {};
            }
            solver.add(GinacToZ3::convert(updated >= updated.subs(up), ctx));
            if (solver.check() == z3::check_result::unsat) {
                proofout << std::endl << "handled " << e << " via eventual strict increase" << std::endl;
                done.push_back(e);
                res.push_back(Relation::normalizeInequality(e.lhs() > updated));
                res.push_back(e);
                todo.erase(it);
                this->equivalent = false;
                print();
                return true;
            }
            solver.pop();
        }
        return false;
    }

    bool eventualWeakIncrease() {
        Z3Context ctx;
        Z3Solver solver(ctx);
        for (const Expression &e: done) {
            solver.add(e.toZ3(ctx));
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver.push();
            const GiNaC::ex &updated = e.lhs().subs(up);
            solver.add(GinacToZ3::convert(e.lhs() <= updated, ctx));
            if (solver.check() != z3::sat) {
                return {};
            }
            solver.add(GinacToZ3::convert(updated > updated.subs(up), ctx));
            if (solver.check() == z3::check_result::unsat) {
                proofout << std::endl << "handled " << e << " via eventual weak increase" << std::endl;
                done.push_back(e);
                res.push_back(Relation::normalizeInequality(e.lhs() >= updated));
                res.push_back(e);
                todo.erase(it);
                this->equivalent = false;
                print();
                return true;
            }
            solver.pop();
        }
        return false;
    }

    void print() {
        proofout << "res:";
        for (const auto &e: this->res) {
            proofout << " " << e;
        }
        proofout << std::endl;
        proofout << "done:";
        for (const auto &e: this->done) {
            proofout << " " << e;
        }
        proofout << std::endl;
        proofout << "todo:";
        for (const auto &e: this->todo) {
            proofout << " " << e;
        }
        proofout << std::endl;
    }

};

struct AccelerationCalculus {

    static option<AccelerationProblem> init(const LinearRule &r, VariableManager &varMan) {
        const ExprSymbol &n = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("n"));
        const option<Recurrence::IteratedUpdates> &sol = Recurrence::iterateUpdates(varMan, {r.getUpdate()}, n);
        if (!sol || sol.get().validityBound > 1) {
            proofout.headline("Failed to compute closed form");
            return {};
        }
        assert(sol.get().refinement.empty());
        assert(sol.get().updates.size() == 1);
        return AccelerationProblem::init(r, varMan, sol.get().updates[0], n);
    }

    static option<AccelerationProblem> solve(AccelerationProblem &p) {
        p.simplify();
        if (p.solved()) {
            return p;
        } else {
            return {};
        }
    }

};
