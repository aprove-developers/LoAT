#include "accelerationproblem.hpp"
#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"

AccelerationProblem::AccelerationProblem(
        const GuardList &res,
        const GuardList &done,
        const GuardList &todo,
        const Subs &up,
        const Subs &closed,
        const Expr &cost,
        const Expr &iteratedCost,
        const Var &n,
        const uint validityBound,
        const VariableManager &varMan): res(res), done(done), todo(todo), up(up), closed(closed), cost(iteratedCost), n(n), validityBound(validityBound), varMan(varMan) {
    this->solver = SmtFactory::solver(Smt::chooseLogic<Subs>({todo}, {up, closed}), varMan);
    this->nonterm = Smt::isImplication(buildAnd(todo), buildLit(cost > 0), varMan);
}

AccelerationProblem AccelerationProblem::init(
        const LinearRule &r,
        const VariableManager &varMan,
        const Subs &closed,
        const Expr &iteratedCost,
        const Var &n,
        const uint validityBound) {
    const GuardList &todo = normalize(r.getGuard());
    AccelerationProblem res({}, {}, todo, r.getUpdate(), closed, r.getCost(), iteratedCost, n, validityBound, varMan);
    while (res.recurrence());
    return res;
}

option<AccelerationProblem> AccelerationProblem::init(const LinearRule &r, VariableManager &varMan) {
    const Var &n = varMan.addFreshTemporaryVariable("n");
    const option<Recurrence::Result> &res = Recurrence::iterateRule(varMan, r, n);
    if (res) {
        return {AccelerationProblem::init(r, varMan, res->update, res->cost, n, res->validityBound)};
    } else {
        return {};
    }
}

GuardList AccelerationProblem::normalize(const GuardList &g) {
    GuardList res;
    for (const Rel &rel: g) {
        if (rel.isEq()) {
            res.push_back(rel.lhs() - rel.rhs() >= 0);
            res.push_back(rel.rhs() - rel.lhs() >= 0);
        } else {
            res.push_back(rel.toG().makeRhsZero());
        }
    }
    return res;
}

bool AccelerationProblem::monotonicity() {
    for (auto it = todo.begin(); it != todo.end(); it++) {
        const Rel &rel = *it;
        solver->push();
        solver->add(rel.subs(up));
        if (solver->check() != Smt::Sat) {
            solver->pop();
            return false;
        }
        solver->add(!rel);
        if (solver->check() == Smt::Unsat) {
            proof.newline();
            proof.append(std::stringstream() << "handled " << rel << " via monotonic decrease");
            done.push_back(rel);
            res.push_back(rel.subs(closed).subs(Subs(n, n-1)));
            nonterm = false;
            solver->pop();
            solver->add(rel);
            todo.erase(it);
            print();
            return true;
        }
        solver->pop();
    }
    return false;
}

bool AccelerationProblem::recurrence() {
    for (auto it = todo.begin(); it != todo.end(); it++) {
        const Rel &rel = *it;
        solver->push();
        solver->add(rel);
        if (solver->check() != Smt::Sat) {
            solver->pop();
            return false;
        }
        solver->add(!rel.subs(up));
        if (solver->check() == Smt::Unsat) {
            proof.newline();
            proof.append(std::stringstream() << "handled " << rel << " via monotonic increase");
            done.push_back(rel);
            res.push_back(rel);
            solver->pop();
            solver->add(rel);
            todo.erase(it);
            print();
            return true;
        }
        solver->pop();
    }
    return false;
}

bool AccelerationProblem::eventualWeakDecrease() {
    for (auto it = todo.begin(); it != todo.end(); it++) {
        const Rel &rel = *it;
        solver->push();
        const Expr &updated = rel.lhs().subs(up);
        solver->add(rel.lhs() >= updated);
        if (solver->check() != Smt::Sat) {
            solver->pop();
            return false;
        }
        solver->add(updated < updated.subs(up));
        if (solver->check() == Smt::Unsat) {
            solver->pop();
            solver->push();
            const Rel &newCond = rel.subs(closed).subs(Subs(n, n-1));
            solver->add(rel);
            solver->add(newCond);
            if (solver->check() == Smt::Sat) {
                proof.newline();
                proof.append(std::stringstream() << "handled " << rel << " via eventual decrease");
                done.push_back(rel);
                res.push_back(rel);
                res.push_back(newCond);
                nonterm = false;
                solver->pop();
                solver->add(rel);
                todo.erase(it);
                print();
                return true;
            }
        }
        solver->pop();
    }
    return false;
}

bool AccelerationProblem::solved() const {
    return todo.empty();
}

ProofOutput AccelerationProblem::getProof() const {
    assert(solved());
    return proof;
}

GuardList AccelerationProblem::getAcceleratedGuard() const {
    assert(solved());
    return res;
}

Expr AccelerationProblem::getAcceleratedCost() const {
    assert(solved());
    return cost;
}

Subs AccelerationProblem::getClosedForm() const {
    return closed;
}

Var AccelerationProblem::getIterationCounter() const {
    return n;
}

bool AccelerationProblem::witnessesNonterm() const {
    assert(solved());
    return nonterm;
}

void AccelerationProblem::simplifyEquivalently() {
    while (recurrence() || monotonicity() || eventualWeakDecrease());
    if (solved() && !nonterm) {
        this->res.push_back(n >= validityBound);
    }
}

bool AccelerationProblem::eventualWeakIncrease() {
    for (auto it = todo.begin(); it != todo.end(); it++) {
        const Rel &rel = *it;
        solver->push();
        const Expr &updated = rel.lhs().subs(up);
        solver->add(rel.lhs() <= updated);
        if (solver->check() != Smt::Sat) {
            solver->pop();
            return false;
        }
        solver->add(updated > updated.subs(up));
        if (solver->check() == Smt::Unsat) {
            solver->pop();
            solver->push();
            const Rel &newCond = updated - rel.lhs() >= 0;
            solver->add(rel);
            solver->add(newCond);
            if (solver->check() == Smt::Sat) {
                proof.newline();
                proof.append(std::stringstream() << "handled " << rel << " via eventual increase");
                done.push_back(rel);
                res.push_back(newCond);
                res.push_back(rel);
                this->equivalent = false;
                solver->pop();
                solver->add(rel);
                print();
                todo.erase(it);
                return true;
            }
        }
        solver->pop();
    }
    return false;
}

void AccelerationProblem::print() {
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
