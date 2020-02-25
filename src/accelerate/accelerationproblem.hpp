#include <ginac/ex.h>
#include "../its/types.hpp"
#include "../its/rule.hpp"
#include "../util/option.hpp"
#include "../expr/relation.hpp"
#include "../its/variablemanager.hpp"
#include "../accelerate/recurrence/recurrence.hpp"
#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"
#include "../analysis/preprocess.hpp"
#include "../util/proofoutput.hpp"

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
    ProofOutput proof;
    const VariableManager varMan;

    AccelerationProblem(
            const GuardList &res,
            const GuardList &done,
            const GuardList &todo,
            const GiNaC::exmap &up,
            const option<GiNaC::exmap> &closed,
            const Expression &cost,
            const ExprSymbol &n,
            const VariableManager &varMan): res(res), done(done), todo(todo), up(up), closed(closed), cost(cost), n(n), varMan(varMan) {
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
        AccelerationProblem res({}, {}, normalize(guard), up, closedSubs, cost, n, varMan);
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
        std::unique_ptr<Smt> solver = SmtFactory::solver(varMan);
        for (const Expression &e: done) {
            solver->add(e);
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver->push();
            solver->add(e.subs(up));
            if (solver->check() != Smt::Sat) {
                return false;
            }
            solver->add(e.lhs() <= 0);
            if (solver->check() == Smt::Unsat) {
                proof.newline();
                proof.append(std::stringstream() << "handled " << e.toString() << " via monotonic decrease");
                done.push_back(e);
                res.push_back(e.subs(closed.get()).subs({{n, n-1}}));
                todo.erase(it);
                print();
                nonterm = false;
                return true;
            }
            solver->pop();
        }
        return false;
    }

    bool recurrence() {
        std::unique_ptr<Smt> solver = SmtFactory::solver(varMan);
        for (const Expression &e: done) {
            solver->add(e);
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver->push();
            solver->add(e);
            if (solver->check() != Smt::Sat) {
                return false;
            }
            solver->add(e.subs(up).lhs() <= 0);
            if (solver->check() == Smt::Unsat) {
                proof.newline();
                proof.append(std::stringstream() << "handled " << e << " via monotonic increase");
                done.push_back(e);
                res.push_back(e);
                todo.erase(it);
                print();
                return true;
            }
            solver->pop();
        }
        return false;
    }

    bool eventualWeakDecrease() {
        if (!closed) {
            return false;
        }
        std::unique_ptr<Smt> solver = SmtFactory::solver(varMan);
        for (const Expression &e: done) {
            solver->add(e);
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver->push();
            const GiNaC::ex &updated = e.lhs().subs(up);
            solver->add(e.lhs() >= updated);
            if (solver->check() != Smt::Sat) {
                return false;
            }
            solver->add(updated < updated.subs(up));
            if (solver->check() == Smt::Unsat) {
                solver->pop();
                solver->push();
                const Expression &newCond = e.subs(closed.get()).subs({{n, n-1}});
                solver->add(e);
                solver->add(newCond);
                if (solver->check() == Smt::Sat) {
                    solver->pop();
                    proof.newline();
                    proof.append(std::stringstream() << "handled " << e << " via eventual decrease");
                    done.push_back(e);
                    res.push_back(e);
                    res.push_back(newCond);
                    todo.erase(it);
                    print();
                    nonterm = false;
                    return true;
                }
            }
            solver->pop();
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
        std::unique_ptr<Smt> solver = SmtFactory::solver(varMan);
        for (const Expression &e: done) {
            solver->add(e);
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Expression &e = *it;
            solver->push();
            const GiNaC::ex &updated = e.lhs().subs(up);
            solver->add(e.lhs() <= updated);
            if (solver->check() != Smt::Sat) {
                return false;
            }
            solver->add(updated > updated.subs(up));
            if (solver->check() == Smt::Unsat) {
                solver->pop();
                solver->push();
                const Expression &newCond = Relation::normalizeInequality(e.lhs() <= updated);
                solver->add(e);
                solver->add(newCond);
                if (solver->check() == Smt::Sat) {
                    solver->pop();
                    proof.newline();
                    proof.append(std::stringstream() << "handled " << e << " via eventual increase");
                    done.push_back(e);
                    res.push_back(newCond);
                    res.push_back(e);
                    todo.erase(it);
                    this->equivalent = false;
                    print();
                    return true;
                }
            }
            solver->pop();
        }
        return false;
    }

    void print() {
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
        proof.append(s);
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
