#include "accelerationproblem.hpp"
#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../util/relevantvariables.hpp"
#include "../../qelim/redlog.hpp"

AccelerationProblem::AccelerationProblem(
        const BoolExpr guard,
        option<const Subs&> closed,
        const Expr &iteratedCost,
        const Var &n,
        const unsigned int validityBound,
        ITSProblem &its): closed(closed), iteratedCost(iteratedCost), n(n), guard(guard), validityBound(validityBound), its(its) {
    Smt::Logic logic;
    if (closed) {
        this->proof.append(std::stringstream() << "accelerating " << guard << " wrt. " << closed);
        logic = Smt::chooseLogic<RelSet, Subs>({guard->lits()}, {closed.get()});
    } else {
        this->proof.append(std::stringstream() << "accelerating " << guard);
        logic = Smt::chooseLogic<RelSet, Subs>({guard->lits()}, {});
    }
    this->solver = SmtFactory::modelBuildingSolver(logic, its);
    this->solver->add(guard);
}

option<AccelerationProblem> AccelerationProblem::init(const LinearRule &r, ITSProblem &its) {
    const Var &n = its.addFreshTemporaryVariable("n");
    const option<Recurrence::Result> &res = Recurrence::iterateRule(its, r, n);
    if (res) {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        option<const Subs&>(res->update),
                        res->cost,
                        n,
                        res->validityBound,
                        its)};
    } else {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        option<const Subs&>(),
                        r.getCost(),
                        n,
                        0,
                        its)};
    }
}

AccelerationProblem AccelerationProblem::initForRecurrentSet(const LinearRule &r, ITSProblem &its) {
    return AccelerationProblem(
                r.getGuard()->toG(),
                option<const Subs&>(),
                r.getCost(),
                its.addFreshTemporaryVariable("n"),
                0,
                its);
}

std::vector<AccelerationProblem::Result> AccelerationProblem::computeRes() {
    Var m = its.getFreshUntrackedSymbol("m", Expr::Int);
    BoolExpr matrix = guard->subs(closed.get());
    auto qelim = Qelim::solver(its);
    QuantifiedFormula q = matrix->quantify({Quantifier(Quantifier::Type::Forall, {n}, {{n, 0}}, {})});
    option<Qelim::Result> res = qelim->qe(q);
    std::vector<AccelerationProblem::Result> ret;
    if (res && res->exact) {
        ret.push_back(Result(res->qf, true, true));
    } else {
        if (res && res->qf != False) {
            ret.push_back(Result(res->qf, false, true));
        }
        matrix = guard->subs(closed.get())->subs({n, m});
        q = matrix->quantify({Quantifier(Quantifier::Type::Forall, {m}, {{m, validityBound}}, {{m, n-1}})});
        res = qelim->qe(q);
        if (res && res->qf != False) {
            ret.push_back(Result(res->qf, res->exact, false));
        }
    }
    return ret;
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
