#ifndef ACCELERATION_PROBLEM
#define ACCELERATION_PROBLEM

#include "../../its/types.hpp"
#include "../../its/rule.hpp"
#include "../../util/option.hpp"
#include "../../its/variablemanager.hpp"
#include "../../util/proof.hpp"
#include "../../smt/smt.hpp"

class AccelerationProblem {

private:

    struct Entry {
        RelSet dependencies;
        BoolExpr formula;
        bool nonterm;
    };

    using Res = RelMap<std::vector<Entry>>;

    Res res;
    option<RelMap<Entry>> solution;
    RelSet todo;
    Subs up;
    Subs closed;
    Expr cost;
    Var n;
    BoolExpr guard;
    uint validityBound;
    Proof proof;
    std::unique_ptr<Smt> solver;
    RelMap<BoolExpr> guardWithout;
    const VariableManager varMan;

    AccelerationProblem(
            const BoolExpr &guard,
            const Subs &up,
            const Subs &closed,
            const Expr &iteratedCost,
            const Var &n,
            const uint validityBound,
            const VariableManager &varMan);

    void monotonicity();
    void recurrence();
    void eventualWeakDecrease();
    RelSet findConsistentSubset(const BoolExpr &e) const;
    uint store(const Rel &rel, const RelSet &deps, const BoolExpr &formula, bool nonterm = false);
    BoolExpr getGuardWithout(const Rel &rel);

public:

    struct Result {
        BoolExpr newGuard;
        bool witnessesNonterm;
    };

    static option<AccelerationProblem> init(const LinearRule &r, VariableManager &varMan);
    option<Result> computeRes();
    Proof getProof() const;
    Expr getAcceleratedCost() const;
    Subs getClosedForm() const;
    Var getIterationCounter() const;

};

#endif
