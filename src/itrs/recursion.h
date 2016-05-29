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
              const std::set<const RightHandSide*> &rightHandSides,
              std::set<const RightHandSide*> &wereUsed,
              std::vector<RightHandSide> &result);

    bool solve();

private:
    bool solveRecursionInOneVar();
    bool findRecursions();
    bool findRealVars(const TT::Expression &term);
    bool findBaseCases();
    bool baseCasesAreSufficient();
    bool solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc);

private:
    // paramters passed to this object
    const ITRSProblem &itrs;
    const FunctionSymbolIndex funSymbolIndex;
    std::set<const RightHandSide*> rightHandSides;
    std::set<const RightHandSide*> &wereUsed;
    std::vector<RightHandSide> &result;
    const FunctionSymbol& funSymbol;

    // findRealVars()
    // stores the indices of the "real" recursion variables
    std::set<int> realVars;

    // attributes
    std::vector<const RightHandSide*> recursions;
    const RightHandSide *recursion;
    int realVarIndex;
    VariableIndex realVar;
    ExprSymbol realVarGiNaC;
    std::map<Purrs::index_type,const RightHandSide*> baseCases;
};

#endif // RECURSION_H