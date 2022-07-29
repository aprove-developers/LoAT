#include "accelerationproblem.hpp"
#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../util/relevantvariables.hpp"
#include "../../qelim/redlog.hpp"
#include "../../nonterm/nontermproblem.hpp"

AccelerationProblem::AccelerationProblem(
        const BoolExpr &guard,
        const Subs &up,
        const Expr &cost,
        option<const Subs&> closed,
        const Expr &iteratedCost,
        const Var &n,
        const unsigned int validityBound,
        ITSProblem &its): closed(closed), iteratedCost(iteratedCost), n(n), guard(guard), up(up), cost(cost), validityBound(validityBound), its(its) {}

option<AccelerationProblem> AccelerationProblem::init(const LinearRule &r, ITSProblem &its) {
    const Var &n = its.addFreshTemporaryVariable("n");
    const option<Recurrence::Result> &res = Recurrence::iterateRule(its, r, n);
    if (res) {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        r.getCost(),
                        option<const Subs&>(res->update),
                        res->cost,
                        n,
                        res->validityBound,
                        its)};
    } else {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        r.getCost(),
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
                r.getUpdate(),
                r.getCost(),
                option<const Subs&>(),
                r.getCost(),
                its.addFreshTemporaryVariable("n"),
                0,
                its);
}

std::vector<AccelerationProblem::Result> AccelerationProblem::computeRes() {
    if (!closed || !closed->isPoly() || !guard->isPolynomial()) {
        auto nt = NontermProblem::init(guard, up, cost, its);
        const auto res = nt.computeRes();
        if (res) {
            return {Result(res->newGuard, nt.getProof(), res->exact, true)};
        }
        if (!closed) {
            return {};
        }
    }
    Var m = its.getFreshUntrackedSymbol("m", Expr::Int);
    BoolExpr matrix = guard->subs(closed.get());
    auto qelim = Qelim::solver(its);
    QuantifiedFormula q = matrix->quantify({Quantifier(Quantifier::Type::Forall, {n}, {{n, 0}}, {})});
    option<Qelim::Result> res = qelim->qe(q);
    std::vector<AccelerationProblem::Result> ret;
    if (res && res->qf != False) {
        Proof proof;
        proof.append(std::stringstream() << "proved non-termination of " << guard << " via quantifier elimination");
        proof.append(std::stringstream() << "quantified formula: " << q);
        proof.append(std::stringstream() << "quantifier-free formula: " << res->qf);
        proof.append(std::stringstream() << "QE proof:");
        proof.concat(res->proof);
        ret.push_back(Result(res->qf, proof, res->exact, true));
        if (res->exact) {
            return ret;
        }
    }
    matrix = guard->subs(closed.get())->subs({n, m});
    q = matrix->quantify({Quantifier(Quantifier::Type::Forall, {m}, {{m, validityBound}}, {{m, n-1}})});
    res = qelim->qe(q);
    if (res && res->qf != False) {
        Proof proof;
        proof.append(std::stringstream() << "accelerated " << guard << " w.r.t. " << closed << " via quantifier elimination");
        proof.append(std::stringstream() << "quantified formula: " << q);
        proof.append(std::stringstream() << "quantifier-free formula: " << res->qf);
        proof.concat(res->proof);
        ret.push_back(Result(res->qf & (n >= validityBound), proof, res->exact, false));
    }
    return ret;
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
