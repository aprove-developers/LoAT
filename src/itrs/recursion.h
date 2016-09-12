#ifndef RECURSION_H
#define RECURSION_H

#include <map>
#include <set>

#include "expression.h"
#include "term.h"
#include "recursiongraph.h"

namespace Purrs = Parma_Recurrence_Relation_Solver;

class FunctionSymbol;
class ITRSProblem;

typedef std::map<Purrs::index_type,int> BaseCaseIndexMap;
typedef std::map<Purrs::index_type,RightHandSide> BaseCaseRhsMap;

class Recursion {
public:
    Recursion(ITRSProblem &itrs,
              FunctionSymbolIndex funSymbolIndex,
              const std::set<const RightHandSide*> &rightHandSides,
              std::set<const RightHandSide*> &wereUsed,
              std::vector<RightHandSide> &result);

    bool solve();

private:
    // search for possible recursion and place them in "recursions"
    bool findRecursions();
    bool findBaseCases();

    // for every rhs in "recursions", every call in the term is moved to the guard
    void moveRecursiveCallsToGuard();

    // instantiates all non-chaining free variables of rules in "recursions"
    void instantiateFreeVariables();
    bool instantiateAFreeVariableOf(const RightHandSide &rhs);

    void evaluateSpecificRecursiveCalls();
    bool evaluateASpecificRecursiveCallOf(const RightHandSide &rhs);

    std::set<int> findRealVars(const RightHandSide &rhs);

    void solveRecursionWithMainVar(const RightHandSide &rhs, int mainVarIndex);

    BaseCaseIndexMap analyzeBaseCases(ExprSymbol mainVar);

    bool baseCasesMatch(const BaseCaseRhsMap &bcs, const RightHandSide &recursion);


    bool solveRecursionInOneVar();
    bool solveRecursionInTwoVars();
    void evaluateConstantRecursiveCalls();
    bool updatesHaveConstDifference(const TT::Expression &term) const;
    bool updatesHaveConstSum(const TT::Expression &term) const;
    bool solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc);
    bool removingSelfReferentialGuardIsSound(const TT::ExpressionVector &srGuard) const;
    void mergeBaseCaseGuards(TT::ExpressionVector &recGuard) const;

    void dumpRightHandSides() const;

private:
    // paramters passed to this object
    ITRSProblem &itrs;
    const FunctionSymbolIndex funSymbolIndex;
    std::set<const RightHandSide*> rightHandSides;
    std::set<const RightHandSide*> &wereUsed;
    std::vector<RightHandSide> &result;
    const FunctionSymbol& funSymbol;

    // attributes
    std::vector<RightHandSide> recursions;
    std::vector<RightHandSide> baseCases;
    std::vector<RightHandSide> closedForms;
};

#endif // RECURSION_H