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
        bool active;

        bool subsumes(const Entry &that) const;

    };

    using Res = RelMap<std::vector<Entry>>;

    Res res;
    option<RelMap<Entry>> solution;
    RelSet todo;
    Subs up;
    Subs closed;
    Expr cost;
    Expr iteratedCost;
    Var n;
    BoolExpr guard;
    uint validityBound;
    Proof proof;
    std::unique_ptr<Smt> solver;
    VariableManager &varMan;

    AccelerationProblem(
            const BoolExpr guard,
            const Subs &up,
            const Subs &closed,
            const Expr &cost,
            const Expr &iteratedCost,
            const Var &n,
            const uint validityBound,
            VariableManager &varMan);

    void monotonicity();
    void recurrence();
    void eventualWeakDecrease();
    RelSet findConsistentSubset(const BoolExpr e) const;
    option<uint> store(const Rel &rel, const RelSet &deps, const BoolExpr formula, bool nonterm = false);

public:

    struct Result {
        BoolExpr newGuard;
        bool witnessesNonterm;
    };

    static option<AccelerationProblem> init(const LinearRule &r, VariableManager &varMan);
    std::vector<Result> computeRes();
    BoolExpr buildRes(const Model &model, const std::map<Rel, std::vector<BoolExpr>> &entryVars);
    Proof getProof() const;
    Expr getAcceleratedCost() const;
    Subs getClosedForm() const;
    Var getIterationCounter() const;

private:

    bool checkCycle(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars);

};

#endif
