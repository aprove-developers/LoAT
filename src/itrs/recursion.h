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

class ORightHandSide : public RightHandSide {
public:
    ORightHandSide() {
    }
    ORightHandSide(const ORightHandSide &rhs)
        : RightHandSide(rhs), origins(rhs.origins) {
    }
    ORightHandSide& operator=(const ORightHandSide &rhs) {
        RightHandSide::operator=(rhs);
        origins = rhs.origins;
    }
    ORightHandSide(const RightHandSide &rhs)
        : RightHandSide(rhs) {
    }
    ORightHandSide& operator=(const RightHandSide &rhs) {
        RightHandSide::operator=(rhs);
        origins.clear();
    }

public:
    std::set<const RightHandSide*> origins;
};

struct VariableConfig {
    VariableConfig() {
        error = false;
    }

    GiNaC::exmap sub;
    GiNaC::exmap reSub;
    bool error;
};

typedef std::map<Purrs::index_type,Purrs::Expr> PurrsBaseCases;
typedef std::map<Purrs::index_type,int> BaseCaseIndexMap;
typedef std::map<Purrs::index_type,ORightHandSide> BaseCaseRhsMap;
typedef std::map<ExprSymbol, TT::Expression, GiNaC::ex_is_less> RecursiveCallMap;
typedef std::vector<GiNaC::exmap> UpdateVector;

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

    void solveRecursionWithMainVar(const ORightHandSide &rhs, const std::set<int> &realVars, int mainVarIndex);

    // for every rhs in "recursions", every call in the term is moved to the guard
    void moveRecursiveCallsToGuard();

    // instantiates all non-chaining free variables of rules in "recursions"
    void instantiateFreeVariables();
    bool instantiateAFreeVariableOf(const ORightHandSide &rhs);

    void evaluateSpecificRecursiveCalls();
    bool evaluateASpecificRecursiveCallOf(const ORightHandSide &rhs);

    std::set<int> findRealVars(const RightHandSide &rhs);

    BaseCaseIndexMap analyzeBaseCases(ExprSymbol mainVar);

    bool baseCasesMatch(const BaseCaseRhsMap &bcs,
                        const UpdateVector &updates,
                        const TT::ExpressionVector &normalGuard);

    VariableConfig generateMultiVarConfig(const UpdateVector &updates,
                                          const std::set<int> &realVars,
                                          int mainVarIndex);
    bool updatesHaveConstDifference(const UpdateVector &updates, int mainVarIndex, int varIndex) const;
    bool updatesHaveConstSum(const UpdateVector &updates, int mainVarIndex, int varIndex) const;

    bool selfReferentialGuardIsInductivelyValid(const TT::ExpressionVector &normalGuard,
                                                const RecursiveCallMap &recCallMap,
                                                const BaseCaseRhsMap &baseCaseMap,
                                                const TT::ExpressionVector &srGuard) const;

    bool computeClosedFormOfTheRHSs(TT::Expression &closed,
                                    const RightHandSide &recursion,
                                    int mainVarIndex,
                                    const VariableConfig &config,
                                    const RecursiveCallMap &recCallMap,
                                    const BaseCaseRhsMap &baseCaseMap);

    bool computeClosedFormOfTheCosts(Expression &closed,
                                     const TT::Expression &closedRHSs,
                                     const RightHandSide &recursion,
                                     int mainVarIndex,
                                     const VariableConfig &config,
                                     const RecursiveCallMap &recCallMap,
                                     const std::vector<TT::Expression> &recCalls,
                                     const BaseCaseRhsMap &baseCaseMap);

    bool solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc);

    TT::ExpressionVector constructGuard(const TT::ExpressionVector &normalGuard,
                                        const BaseCaseRhsMap &baseCaseMap) const;

    ExprSymbolSet getNonChainingFreeVariablesOf(const RightHandSide &rule) const;
    ExprSymbolSet getChainingVariablesOf(const TT::Expression &ex) const;
    ExprSymbolSet getChainingVariablesOf(const RightHandSide &rule) const;

    void dumpRightHandSides() const;

private:
    // paramters passed to this object
    ITRSProblem &itrs;
    const FunctionSymbolIndex funSymbolIndex;
    const std::set<const RightHandSide*> &rightHandSides;
    std::set<const RightHandSide*> &wereUsed;
    std::vector<RightHandSide> &result;
    const FunctionSymbol& funSymbol;

    // attributes
    std::vector<ORightHandSide> recursions;
    std::vector<ORightHandSide> baseCases;
    std::vector<ORightHandSide> closedForms;
};

#endif // RECURSION_H