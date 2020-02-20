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
    option<GiNaC::exmap> closed;
    Expression cost;
    ExprSymbol n;
    GuardList guard;
    bool equivalent = true;
    bool nonterm = true;
    bool silent = true;

    AccelerationProblem(
            const GuardList &res,
            const GuardList &done,
            const GuardList &todo,
            const GiNaC::exmap &up,
            const option<GiNaC::exmap> &closed,
            const Expression &cost,
            const ExprSymbol &n): res(res), done(done), todo(todo), up(up), closed(closed), cost(cost), n(n) {
        this->res.push_back(n > 0);
    }

    static AccelerationProblem init(
            const UpdateMap &update,
            const GuardList &guard,
            const VariableManager &varMan,
            const option<UpdateMap> &closed,
            const Expression &cost,
            const ExprSymbol &n) {
        const GiNaC::exmap &up = update.toSubstitution(varMan);
        option<GiNaC::exmap> closedSubs;
        if (closed){
            closedSubs = closed.get().toSubstitution(varMan);
        } else {
            closedSubs = {};
        }
        AccelerationProblem res({}, {}, normalize(guard), up, closedSubs, cost, n);
        while (res.recurrence());
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
        if (!closed) {
            return false;
        }
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
                if (!silent) {
                    proofout.newline();
                    proofout.append(std::stringstream() << "handled " << e.toString() << " via monotonic decrease");
                }
                done.push_back(e);
                res.push_back(e.subs(closed.get()).subs({{n, n-1}}));
                todo.erase(it);
                print();
                nonterm = false;
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
                if (!silent) {
                    proofout.newline();
                    proofout.append(std::stringstream() << "handled " << e << " via monotonic increase");
                }
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

    bool eventualWeakDecrease() {
        if (!closed) {
            return false;
        }
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
                solver.pop();
                solver.push();
                const Expression &newCond = e.subs(closed.get()).subs({{n, n-1}});
                solver.add(e.toZ3(ctx));
                solver.add(newCond.toZ3(ctx));
                if (solver.check() == z3::sat) {
                    solver.pop();
                    if (!silent) {
                        proofout.newline();
                        proofout.append(std::stringstream() << "handled " << e << " via eventual decrease");
                    }
                    done.push_back(e);
                    res.push_back(e);
                    res.push_back(newCond);
                    todo.erase(it);
                    print();
                    nonterm = false;
                    return true;
                }
            }
            solver.pop();
        }
        return false;
    }

    bool solved() {
        return todo.empty();
    }

    void simplifyEquivalently() {
        while (true) {
            if (!recurrence() && !monotonicity() && !eventualWeakDecrease()) {
                break;
            }
        }
    }

    void simplifyNonterm() {
        while (true) {
            if (!recurrence() && !eventualWeakIncrease()) {
                break;
            }
        }
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
                return false;
            }
            solver.add(GinacToZ3::convert(updated > updated.subs(up), ctx));
            if (solver.check() == z3::check_result::unsat) {
                solver.pop();
                solver.push();
                const Expression &newCond = Relation::normalizeInequality(e.lhs() <= updated);
                solver.add(e.toZ3(ctx));
                solver.add(newCond.toZ3(ctx));
                if (solver.check() == z3::sat) {
                    solver.pop();
                    if (!silent) {
                        proofout.newline();
                        proofout.append(std::stringstream() << "handled " << e << " via eventual increase");
                    }
                    done.push_back(e);
                    res.push_back(newCond);
                    res.push_back(e);
                    todo.erase(it);
                    this->equivalent = false;
                    print();
                    return true;
                }
            }
            solver.pop();
        }
        return false;
    }

    void print() {
        if (!silent) {
            std::stringstream s;
            s << "res:";
            for (const auto &e: this->res) {
                s << " " << e;
            }
            s << std::endl << "done:";
            for (const auto &e: this->done) {
                s << " " << e;
            }
            s << std::endl << "todo:";
            for (const auto &e: this->todo) {
                s << " " << e;
            }
            proofout.append(s);
        }
    }

};

struct AccelerationCalculus {

    static AccelerationProblem init(const LinearRule &r, VariableManager &varMan) {
        const ExprSymbol &n = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("n"));
        UpdateMap closed = r.getUpdate();
        Expression cost = r.getCost();
        GuardList guard = r.getGuard();
        const option<unsigned int> &validityBound = Recurrence::iterateUpdateAndCost(varMan, closed, cost, guard, n);
        if (!validityBound) {
            proofout.headline("Failed to compute closed form");
            return AccelerationProblem::init(r.getUpdate(), guard, varMan, {}, cost, n);
        } else {
            return AccelerationProblem::init(r.getUpdate(), guard, varMan, closed, cost, n);
        }
    }

    static option<AccelerationProblem> solveEquivalently(AccelerationProblem &p) {
        p.simplifyEquivalently();
        if (p.solved()) {
            return p;
        } else {
            return {};
        }
    }

    static option<AccelerationProblem> solveNonterm(AccelerationProblem &p) {
        p.simplifyNonterm();
        if (p.solved()) {
            return p;
        } else {
            return {};
        }
    }

};
