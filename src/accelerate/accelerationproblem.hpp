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
#include "../util/timeout.hpp"

struct AccelerationProblem {
    GuardList res;
    GuardList done;
    GuardList todo;
    ExprMap up;
    option<ExprMap> closed;
    Expression cost;
    ExprSymbol n;
    GuardList guard;
    uint validityBound;
    bool equivalent = true;
    bool nonterm = true;
    ProofOutput proof;
    std::unique_ptr<Smt> solver;
    const VariableManager varMan;

    AccelerationProblem(
            const GuardList &res,
            const GuardList &done,
            const GuardList &todo,
            const ExprMap &up,
            const option<ExprMap> &closed,
            const Expression &cost,
            const ExprSymbol &n,
            const uint validityBound,
            const VariableManager &varMan): res(res), done(done), todo(todo), up(up), closed(closed), cost(cost), n(n), validityBound(validityBound), varMan(varMan) {
        this->res.push_back(n >= validityBound);
        if (closed) {
            this->solver = SmtFactory::solver(Smt::chooseLogic<ExprMap>({todo}, {up, closed.get()}), varMan);
        } else {
            this->solver = SmtFactory::solver(Smt::chooseLogic<ExprMap>({todo}, {up}), varMan);
        }
    }

    static AccelerationProblem init(
            const UpdateMap &update,
            const GuardList &guard,
            const VariableManager &varMan,
            const option<UpdateMap> &closed,
            const Expression &cost,
            const ExprSymbol &n,
            const uint validityBound) {
        const ExprMap &up = update.toSubstitution(varMan);
        option<ExprMap> closedSubs;
        if (closed){
            closedSubs = closed.get().toSubstitution(varMan);
        } else {
            closedSubs = {};
        }
        const GuardList &todo = normalize(guard);
        AccelerationProblem res({}, {}, todo, up, closedSubs, cost, n, validityBound, varMan);
        while (res.recurrence());
        return res;
    }

    static GuardList normalize(const GuardList &g) {
        GuardList res;
        for (const Rel &rel: g) {
            if (rel.getOp() == Rel::eq) {
                res.push_back((rel.lhs() >= rel.rhs()).normalizeInequality());
                res.push_back((rel.lhs() <= rel.rhs()).normalizeInequality());
            } else {
                res.push_back(rel.normalizeInequality());
            }
        }
        return res;
    }

    bool monotonicity() {
        if (!closed) {
            return false;
        }
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Rel &rel = *it;
            solver->push();
            solver->add(rel.subs(up));
            if (solver->check() != Smt::Sat) {
                solver->pop();
                return false;
            }
            solver->add(rel.lhs() <= 0);
            if (solver->check() == Smt::Unsat) {
                proof.newline();
                proof.append(std::stringstream() << "handled " << rel << " via monotonic decrease");
                done.push_back(rel);
                res.push_back(rel.subs(closed.get()).subs(ExprMap(n, n-1)));
                print();
                nonterm = false;
                solver->pop();
                solver->add(rel);
                todo.erase(it);
                return true;
            }
            solver->pop();
        }
        return false;
    }

    bool recurrence() {
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Rel &rel = *it;
            solver->push();
            solver->add(rel);
            if (solver->check() != Smt::Sat) {
                solver->pop();
                return false;
            }
            solver->add(rel.subs(up).lhs() <= 0);
            if (solver->check() == Smt::Unsat) {
                proof.newline();
                proof.append(std::stringstream() << "handled " << rel << " via monotonic increase");
                done.push_back(rel);
                res.push_back(rel);
                print();
                solver->pop();
                solver->add(rel);
                todo.erase(it);
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
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Rel &rel = *it;
            solver->push();
            const Expression &updated = rel.lhs().subs(up);
            solver->add(rel.lhs() >= updated);
            if (solver->check() != Smt::Sat) {
                solver->pop();
                return false;
            }
            solver->add(updated < updated.subs(up));
            if (solver->check() == Smt::Unsat) {
                solver->pop();
                solver->push();
                const Rel &newCond = rel.subs(closed.get()).subs(ExprMap(n, n-1));
                solver->add(rel);
                solver->add(newCond);
                if (solver->check() == Smt::Sat) {
                    proof.newline();
                    proof.append(std::stringstream() << "handled " << rel << " via eventual decrease");
                    done.push_back(rel);
                    res.push_back(rel);
                    res.push_back(newCond);
                    print();
                    nonterm = false;
                    solver->pop();
                    solver->add(rel);
                    todo.erase(it);
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
        for (auto it = todo.begin(); it != todo.end(); it++) {
            const Rel &rel = *it;
            solver->push();
            const Expression &updated = rel.lhs().subs(up);
            solver->add(rel.lhs() <= updated);
            if (solver->check() != Smt::Sat) {
                solver->pop();
                return false;
            }
            solver->add(updated > updated.subs(up));
            if (solver->check() == Smt::Unsat) {
                solver->pop();
                solver->push();
                const Rel &newCond = (rel.lhs() <= updated).normalizeInequality();
                solver->add(rel);
                solver->add(newCond);
                if (solver->check() == Smt::Sat) {
                    proof.newline();
                    proof.append(std::stringstream() << "handled " << rel << " via eventual increase");
                    done.push_back(rel);
                    res.push_back(newCond);
                    res.push_back(rel);
                    this->equivalent = false;
                    print();
                    solver->pop();
                    solver->add(rel);
                    todo.erase(it);
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
        const option<Recurrence::Result> &res = Recurrence::iterateRule(varMan, r, n);
        const option<UpdateMap> &closed = res ? option<UpdateMap>(res.get().update) : option<UpdateMap>();
        uint validityBound = res ? res->validityBound : 0;
        return AccelerationProblem::init(r.getUpdate(), r.getGuard(), varMan, closed, r.getCost(), n, validityBound);
    }

};
