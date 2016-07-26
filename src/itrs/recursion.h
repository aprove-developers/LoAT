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

typedef std::map<Purrs::index_type,Purrs::Expr> PurrsBaseCases;

class Recursion {
public:
    Recursion(ITRSProblem &itrs,
              FunctionSymbolIndex funSymbolIndex,
              const std::set<const RightHandSide*> &rightHandSides,
              std::set<const RightHandSide*> &wereUsed,
              std::vector<RightHandSide> &result);

    bool solve();

private:
    bool solveRecursionInOneVar();
    bool solveRecursionInTwoVars();
    void substituteFreeVariables();
    void evaluateConstantRecursiveCalls();
    void instantiateACandidate();
    bool updatesHaveConstDifference(const TT::Expression &term) const;
    bool updatesHaveConstSum(const TT::Expression &term) const;
    bool findRecursions();
    std::set<int> findRealVars(const TT::Expression &term);
    bool findBaseCases();
    bool baseCasesAreSufficient();
    bool solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc);
    bool removingSelfReferentialGuardIsSound(const TT::ExpressionVector &srGuard) const;
    void mergeBaseCaseGuards(TT::ExpressionVector &recGuard) const;

private:
    // paramters passed to this object
    ITRSProblem &itrs;
    const FunctionSymbolIndex funSymbolIndex;
    std::set<const RightHandSide*> rightHandSides;
    std::set<const RightHandSide*> &wereUsed;
    std::vector<RightHandSide> &result;
    const FunctionSymbol& funSymbol;

    // attributes
    std::vector<const RightHandSide*> recursions;
    const RightHandSide *recursion;
    RightHandSide recursionCopy;
    TT::ExpressionVector selfReferentialGuard;

    int realVarIndex;
    VariableIndex realVar;
    ExprSymbol realVarGiNaC;

    int realVar2Index;
    VariableIndex realVar2;
    ExprSymbol realVar2GiNaC;

    GiNaC::symbol constDiff;
    // realVar2 -> realVar - constDiff
    // or realVar2 -> constDiff - realVar
    GiNaC::exmap realVarSub;
    std::map<Purrs::index_type,const RightHandSide*> baseCases;

    ExprSymbolSet instCandidates;
    // z -> 5
    GiNaC::exmap instSub;

    GiNaC::exmap freeVarSub;

    TT::Substitution conRecCallsSub;
};

#endif // RECURSION_H