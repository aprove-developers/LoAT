#ifndef ACCELERATION_PROBLEM
#define ACCELERATION_PROBLEM

#include "../../its/types.hpp"
#include "../../its/rule.hpp"
#include "../../util/option.hpp"
#include "../../its/variablemanager.hpp"
#include "../../util/proofoutput.hpp"
#include "../../smt/smt.hpp"

class AccelerationProblem {
private:
    Guard res;
    Guard done;
    Guard todo;
    Subs up;
    Subs closed;
    Expr cost;
    Var n;
    Guard guard;
    uint validityBound;
    bool equivalent = true;
    bool nonterm;
    ProofOutput proof;
    std::unique_ptr<Smt> solver;
    const VariableManager varMan;

    AccelerationProblem(
            const Guard &res,
            const Guard &done,
            const Guard &todo,
            const Subs &up,
            const Subs &closed,
            const Expr &cost,
            const Expr &iteratedCost,
            const Var &n,
            const uint validityBound,
            const VariableManager &varMan);

    static AccelerationProblem init(
            const LinearRule &r,
            const VariableManager &varMan,
            const Subs &closed,
            const Expr &iteratedCost,
            const Var &n,
            const uint validityBound);

    static Guard normalize(const Guard &g);
    bool monotonicity();
    bool recurrence();
    bool eventualWeakDecrease();
    bool eventualWeakIncrease();
    void print();

public:

    static option<AccelerationProblem> init(const LinearRule &r, VariableManager &varMan);
    void simplifyEquivalently();
    bool solved() const;
    bool witnessesNonterm() const;
    ProofOutput getProof() const;
    Guard getAcceleratedGuard() const;
    Expr getAcceleratedCost() const;
    Subs getClosedForm() const;
    Var getIterationCounter() const;

};

#endif
