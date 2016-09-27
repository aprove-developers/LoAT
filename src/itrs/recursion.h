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

/**
 * "OriginRightHandSide"
 * The "origins" set is used to remember which rules were used to create this rule.
 */
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

/**
 * Helper structure that is used for rules with multiple variables,
 * i.e., to substitute an update that has a constant difference or constant sum
 * to the main variable.
 */
struct VariableConfig {
    VariableConfig() {
        error = false;
    }

    GiNaC::exmap sub;
    GiNaC::exmap reSub;
    bool error;
};


// some types that are used
typedef std::map<Purrs::index_type,Purrs::Expr> PurrsBaseCases;
typedef std::map<Purrs::index_type,int> BaseCaseIndexMap;
typedef std::map<Purrs::index_type,ORightHandSide> BaseCaseRhsMap;
typedef std::map<ExprSymbol, TT::Expression, GiNaC::ex_is_less> RecursiveCallMap;
typedef std::vector<GiNaC::exmap> UpdateVector;

/**
 * This class implements the main logic of finding a closed-form rule.
 */
class Recursion {
public:
    /**
     * Constructor
     * @param itrs the itrs problem
     * @param funSymbolIndex the function symbol index for which we try to solve a recursion
     * @param rightHandSides pointers to the right-hand sides that should be considered
     * @param wereUsed pointers to the right-hand sides that were successfully used
     * @param result the resulting closed-form rules are stored in there
     */
    Recursion(ITRSProblem &itrs,
              FunctionSymbolIndex funSymbolIndex,
              const std::set<const RightHandSide*> &rightHandSides,
              std::set<const RightHandSide*> &wereUsed,
              std::vector<RightHandSide> &result);

    /**
     * Try to construct a closed-form rule. This invokes all the helper functions below.
     */
    bool solve();

private:

    /**
     * Search for possible recursion and place them in "recursions".
     */
    bool findRecursions();

    /**
     * Search for possible base cases and place them in "baseCases".
     */
    bool findBaseCases();


    /**
     * The main logic is in this function.
     * @param rhs the recursion
     * @param realVars the set obtained by a call to "findRealVars()"
     * @param mainVarIndex the main variable
     *                     this is a number between 0 and n (where n is the arity of the function symbol)
     *                     this is not an index used by the ITRSProblem class
     */
    void solveRecursionWithMainVar(const ORightHandSide &rhs, const std::set<int> &realVars, int mainVarIndex);

    /**
     * For every rhs in "recursions", every call in the term is moved to the guard.
     */
    void moveRecursiveCallsToGuard();

    /**
     * Instantiates all non-chaining free variables of rules in "recursions".
     */
    void instantiateFreeVariables();

    /**
     * Helper function used by the function above.
     */
    bool instantiateAFreeVariableOf(const ORightHandSide &rhs);

    /**
     * For every rhs in "recursions", this tries to evaluate the specific recursive calls.
     * A recursive call is specific if all passed arguments are integers.
     */
    void evaluateSpecificRecursiveCalls();

    /**
     * Helper function used by the function above.
     */
    bool evaluateASpecificRecursiveCallOf(const ORightHandSide &rhs);

    /**
     * Finds the real vars of the recursion "rhs", i.e., variables that are changed by the update.
     */
    std::set<int> findRealVars(const RightHandSide &rhs);

    /**
     * Derives base cases from the rules in "baseCases",
     * i.e., returns a map int -> base case.
     * @param mainVar the variable that is used as the main variable
     */
    BaseCaseIndexMap analyzeBaseCases(ExprSymbol mainVar);

    /**
     * Checks whether the base cases match the updates and the guard.
     */
    bool baseCasesMatch(const BaseCaseRhsMap &bcs,
                        const UpdateVector &updates,
                        const TT::ExpressionVector &normalGuard);

    /**
     * Helper function used for dealing with multivariate recursions.
     * This function checks if the updates have a constant difference or constant sum to the main variable.
     */
    VariableConfig generateMultiVarConfig(const UpdateVector &updates,
                                          const std::set<int> &realVars,
                                          int mainVarIndex);
    bool updatesHaveConstDifference(const UpdateVector &updates, int mainVarIndex, int varIndex) const;
    bool updatesHaveConstSum(const UpdateVector &updates, int mainVarIndex, int varIndex) const;

    /**
     * Returns true if the self-referential guard could be proven to be inductively valid.
     */
    bool selfReferentialGuardIsInductivelyValid(const ORightHandSide &recursion,
                                                const TT::ExpressionVector &normalGuard,
                                                const TT::ExpressionVector &srGuard,
                                                const RecursiveCallMap &recCallMap,
                                                const UpdateVector &updates,
                                                const BaseCaseRhsMap &baseCaseMap) const;

    /**
     * Helper function used by the function above.
     */
    bool advance(std::vector<int> &selector, const int m) const;

    /**
     * Helper function used by "selfReferentialGuardIsInductivelyValid()".
     */
    void addMu(std::vector<Expression> &addTo,
               const TT::ExpressionVector &normalGuard,
               const TT::ExpressionVector &srGuard,
               const UpdateVector &updates,
               const std::vector<GiNaC::exmap> &renamings,
               const BaseCaseRhsMap &baseCaseMap,
               int i,
               int j) const;

    /**
     * Helper function used by "selfReferentialGuardIsInductivelyValid()".
     */
    Expression getS(const ORightHandSide &recursion,
                    const UpdateVector &updates,
                    const std::vector<GiNaC::exmap> &renamings,
                    const BaseCaseRhsMap &baseCaseMap,
                    int i,
                    int j) const;

    /*
     * Compute a closed form of the right-hand sides.
     */
    bool computeClosedFormOfTheRHSs(TT::Expression &closed,
                                    const RightHandSide &recursion,
                                    int mainVarIndex,
                                    const VariableConfig &config,
                                    const RecursiveCallMap &recCallMap,
                                    const BaseCaseRhsMap &baseCaseMap);

    /*
     * Compute a closed form of the costs.
     */
    bool computeClosedFormOfTheCosts(Expression &closed,
                                     const TT::Expression &closedRHSs,
                                     const RightHandSide &recursion,
                                     int mainVarIndex,
                                     const VariableConfig &config,
                                     const RecursiveCallMap &recCallMap,
                                     const std::vector<TT::Expression> &recCalls,
                                     const BaseCaseRhsMap &baseCaseMap);

    /*
     * Helper function used by the functions above, calls PURRS.
     */
    bool solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc);

    /*
     * Tries to merge the guard of the base case into the guard of the recursion.
     */
    TT::ExpressionVector constructGuard(const TT::ExpressionVector &normalGuard,
                                        const BaseCaseRhsMap &baseCaseMap) const;

    /*
     * More helper functions.
     */
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