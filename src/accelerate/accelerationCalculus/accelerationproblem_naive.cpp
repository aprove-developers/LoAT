#include "accelerationproblem_naive.hpp"
#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../util/relevantvariables.hpp"

AccelerationProblem::AccelerationProblem(
        const BoolExpr guard,
        const Subs &up,
        option<const Subs&> closed,
        const Expr &cost,
        const Expr &iteratedCost,
        const Var &n,
        const unsigned int validityBound,
        ITSProblem &its): todo(guard->lits()), up(up), closed(closed), cost(cost), iteratedCost(iteratedCost), n(n), guard(guard), validityBound(validityBound), its(its) {
    std::vector<Subs> subs = closed.map([&up](auto const &closed){return std::vector<Subs>{up, closed};}).get_value_or({up});
    Smt::Logic logic = Smt::chooseLogic<RelSet, Subs>({todo}, subs);
    this->solver = SmtFactory::modelBuildingSolver(logic, its);
    this->solver2 = SmtFactory::modelBuildingSolver(logic, its);
}

option<AccelerationProblem> AccelerationProblem::init(const LinearRule &r, ITSProblem &its) {
    const Var &n = its.addFreshTemporaryVariable("n");
    const option<Recurrence::Result> &res = Recurrence::iterateRule(its, r, n);
    if (res) {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        option<const Subs&>(res->update),
                        r.getCost(),
                        res->cost,
                        n,
                        res->validityBound,
                        its)};
    } else {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        option<const Subs&>(),
                        r.getCost(),
                        r.getCost(),
                        n,
                        0,
                        its)};
    }
}

AccelerationProblem AccelerationProblem::initForRecurrentSet(const LinearRule &r, ITSProblem &its) {
    return AccelerationProblem(
                r.getGuard()->toG(),
                r.getUpdate(),
                option<const Subs&>(),
                r.getCost(),
                r.getCost(),
                its.addFreshTemporaryVariable("n"),
                0,
                its);
}

bool AccelerationProblem::monotonicity(const Rel &rel) {
    if (closed) {
        solver2->push();
        solver2->add(rel.subs(up));
        if (solver2->check() == Smt::Sat) {
            solver2->add(!buildLit(rel));
            if (solver2->check() == Smt::Unsat) {
                Rel newCond = rel.subs(closed.get()).subs(Subs(n, n-1));
                solver->push();
                solver->add(newCond);
                if (solver->check() == Smt::Sat) {
                    res = res & newCond;
                    nonterm = false;
                    std::stringstream ss;
                    ss << "discharged " << rel << " with monotonic decrease, got " << newCond;
                    proof.newline();
                    proof.append(ss);
                    solver2->pop();
                    solver2->add(rel);
                    return true;
                }
                solver->pop();
            }
        }
        solver2->pop();
    }
    return false;
}

bool AccelerationProblem::recurrence(const Rel &rel) {
    solver2->push();
    solver2->add(rel);
    if (solver2->check() == Smt::Sat) {
        solver2->add(!buildLit(rel.subs(up)));
        if (solver2->check() == Smt::Unsat) {
            solver->push();
            solver->add(rel);
            if (solver->check() == Smt::Sat) {
                res = res & rel;
                std::stringstream ss;
                ss << "discharged " << rel << " with monotonic increase, got " << rel;
                proof.newline();
                proof.append(ss);
                solver2->pop();
                solver2->add(rel);
                return true;
            }
            solver->pop();
        }
    }
    solver2->pop();
    return false;
}

bool AccelerationProblem::eventualWeakDecrease(const Rel &rel) {
    if (closed) {
        const Expr &updated = rel.lhs().subs(up);
        const Rel &dec = rel.lhs() >= updated;
        solver2->push();
        solver2->add(dec);
        if (solver2->check() == Smt::Sat) {
            const Rel &dec_dec = updated >= updated.subs(up);
            solver2->add(!dec_dec);
            if (solver2->check() == Smt::Unsat) {
                BoolExpr newCond = buildLit(rel) & rel.subs(closed.get()).subs(Subs(n, n-1));
                solver->push();
                solver->add(newCond);
                if (solver->check() == Smt::Sat) {
                    res = res & newCond;
                    nonterm = false;
                    std::stringstream ss;
                    ss << "discharged " << rel << " with eventual decrease, got " << newCond;
                    proof.newline();
                    proof.append(ss);
                    solver2->pop();
                    solver2->add(rel);
                    return true;
                }
                solver->pop();
            }
        }
        solver2->pop();
    }
    return false;
}

bool AccelerationProblem::eventualWeakIncrease(const Rel &rel) {
    const Expr &updated = rel.lhs().subs(up);
    const Rel &inc = rel.lhs() <= updated;
    solver2->push();
    solver2->add(inc);
    if (solver2->check() == Smt::Sat) {
        const Rel &inc_inc = updated <= updated.subs(up);
        solver2->add(!inc_inc);
        if (solver2->check() == Smt::Unsat) {
            solver->push();
            solver->add(inc);
            if (solver->check() == Smt::Sat) {
                res = res & inc;
                std::stringstream ss;
                ss << "discharged " << rel << " with eventual increase, got " << inc;
                proof.newline();
                proof.append(ss);
                solver2->pop();
                solver2->add(rel);
                return true;
            }
            solver->pop();
        }
    }
    solver2->pop();
    return false;
}

bool AccelerationProblem::fixpoint(const Rel &rel) {
    RelSet eqs;
    VarSet vars = util::RelevantVariables::find(rel.vars(), {up}, True);
    for (const Var& var: vars) {
        eqs.insert(Rel::buildEq(var, Expr(var).subs(up)));
    }
    BoolExpr allEq = buildAnd(eqs);
    if (Smt::check(guard & allEq, its) == Smt::Sat) {
        solver->push();
        solver->add(allEq);
        if (solver->check() == Smt::Sat) {
            std::stringstream ss;
            ss << rel << "discharged " << rel << " with fixpoint, got " << allEq;
            proof.newline();
            proof.append(ss);
            return true;
        }
        solver->pop();
    }
    return false;
}

std::vector<AccelerationProblem::Result> AccelerationProblem::computeRes() {
    this->proof.append(std::stringstream() << "accelerating " << guard << " wrt. " << up);
    solver->add(n >= validityBound);
    solver->push();
    bool changed;
    do {
        changed = false;
        auto it = todo.begin();
        while (it != todo.end()) {
            if (recurrence(*it) || monotonicity(*it) || eventualWeakDecrease(*it) || eventualWeakIncrease(*it) || fixpoint(*it)) {
                done = done & *it;
                it = todo.erase(it);
                changed = true;
            } else {
                it++;
            }
        }
    } while (changed);
    std::vector<Result> result;
    if (todo.empty()) {
        bool positiveCost = Smt::isImplication(guard, buildLit(cost > 0), its);;
        if (nonterm) {
            nonterm = positiveCost;
        }
        result.push_back({res, nonterm});
        if (!nonterm && closed && positiveCost) {
            proof.append("done, trying nonterm");
            todo = guard->lits();
            done = True;
            solver->popAll();
            do {
                changed = false;
                auto it = todo.begin();
                while (it != todo.end()) {
                    if (recurrence(*it) || eventualWeakIncrease(*it) || fixpoint(*it)) {
                        done = done & *it;
                        it = todo.erase(it);
                        changed = true;
                    } else {
                        it++;
                    }
                }
            } while (changed);
            if (todo.empty()) {
                assert(nonterm);
                result.push_back({res, true});
            }
        }
    }
    return result;
}

Proof AccelerationProblem::getProof() const {
    return proof;
}

Expr AccelerationProblem::getAcceleratedCost() const {
    return iteratedCost;
}

option<Subs> AccelerationProblem::getClosedForm() const {
    return closed;
}

Var AccelerationProblem::getIterationCounter() const {
    return n;
}

unsigned int AccelerationProblem::getValidityBound() const {
    return validityBound;
}
