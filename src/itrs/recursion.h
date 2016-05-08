#ifndef RECURSION_H
#define RECURSION_H

#include <map>
#include <set>

#include "expression.h"
#include "term.h"

namespace Purrs = Parma_Recurrence_Relation_Solver;

class FunctionSymbol;
class ITRSProblem;
class RightHandSide;

typedef std::map<Purrs::index_type,Purrs::Expr> PurrsBaseCases;

class Recursion {
public:
    Recursion(const ITRSProblem &itrs,
              FunctionSymbolIndex funSymbolIndex,
              std::set<const RightHandSide*> &rightHandSides,
              TT::Expression &result,
              TT::Expression &cost,
              TT::ExpressionVector &guard);

    bool solve();

private:
    bool findBaseCases();
    bool baseCasesAreSufficient();
    bool solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc);

private:
    // paramters passed to this object
    const ITRSProblem &itrs;
    const FunctionSymbolIndex funSymbolIndex;
    std::set<const RightHandSide*> &rightHandSides;
    TT::Expression &result;
    TT::Expression &cost;
    TT::ExpressionVector &guard;

    // attributes
    const RightHandSide *recursion;
    VariableIndex critVar;
    ExprSymbol critVarGiNaC;
    std::map<Purrs::index_type,const RightHandSide*> baseCases;
};

#endif // RECURSION_H