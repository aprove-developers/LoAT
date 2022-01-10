#ifndef ACCELERATION_PROBLEM_NAIVE
#define ACCELERATION_PROBLEM_NAIVE

#include "../../its/types.hpp"
#include "../../its/rule.hpp"
#include "../../util/option.hpp"
#include "../../its/itsproblem.hpp"
#include "../../util/proof.hpp"
#include "../../smt/smt.hpp"

class AccelerationProblem {

private:

    BoolExpr res;
    RelSet todo;
    BoolExpr done = True;
    Subs up;
    option<Subs> closed;
    Expr cost;
    Expr iteratedCost;
    Var n;
    BoolExpr guard;
    unsigned int validityBound;
    Proof proof;
    std::unique_ptr<Smt> solver;
    ITSProblem &its;

    AccelerationProblem(
            const BoolExpr guard,
            const Subs &up,
            option<const Subs&> closed,
            const Expr &cost,
            const Expr &iteratedCost,
            const Var &n,
            const unsigned int validityBound,
            ITSProblem &its);

    bool monotonicity(const Rel &rel);
    bool recurrence(const Rel &rel);
    bool eventualWeakDecrease(const Rel &rel);
    bool eventualWeakIncrease(const Rel &rel);
    bool fixpoint(const Rel &rel);

public:

    struct Result {
        BoolExpr newGuard;
        bool witnessesNonterm;
    };

    static option<AccelerationProblem> init(const LinearRule &r, ITSProblem &its);
    static AccelerationProblem initForRecurrentSet(const LinearRule &r, ITSProblem &its);
    std::vector<Result> computeRes();
    std::pair<BoolExpr, bool> buildRes(const Model &model, const std::map<Rel, std::vector<BoolExpr>> &entryVars);
    Proof getProof() const;
    Expr getAcceleratedCost() const;
    option<Subs> getClosedForm() const;
    Var getIterationCounter() const;
    unsigned int getValidityBound() const;
    bool nonterm = true;

};

#endif
