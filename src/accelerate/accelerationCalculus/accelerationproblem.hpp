#ifndef ACCELERATION_PROBLEM
#define ACCELERATION_PROBLEM

#include "../../its/types.hpp"
#include "../../its/rule.hpp"
#include "../../util/option.hpp"
#include "../../its/itsproblem.hpp"
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
    RelSet findConsistentSubset(const BoolExpr e) const;
    option<unsigned int> store(const Rel &rel, const RelSet &deps, const BoolExpr formula, bool nonterm = false);

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

private:

    bool checkCycle(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars);
    void encodeAcyclicity(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars);
    using Vars = std::vector<BoolExpr>;
    Model enlargeSolution(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars, const BoolExprSet &soft);
    Smt::Result checkSat(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars, const RelMap<BoolExpr> &boolAbstractionMap);
    option<AccelerationProblem::Result> enlargeSolutionAndGetRes(const std::map<Rel, Vars> &entryVars, const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars, const BoolExprSet &soft);
    bool depsWellFounded(const Rel& rel, bool nontermOnly, bool linearOnly, RelSet seen);

};

#endif
