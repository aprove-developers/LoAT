#ifndef ASYMPTOTICBOUND_H
#define ASYMPTOTICBOUND_H

#include <string>
#include <vector>

#include "expression.h"
#include "guardtoolbox.h"
#include "infinity.h"
#include "itrs.h"
#include "inftyexpression.h"
#include "limitproblem.h"



class AsymptoticBound {
private:
    // internal struct for the return value of getComplexity()
    struct ComplexityResult {
        ComplexityResult() {
            complexity = Expression::ComplexNone;
            upperBound = 0;
            lowerBound = 0;
        }

        GiNaC::exmap solution;
        Complexity complexity;
        int upperBound;
        int lowerBound;
    };

    AsymptoticBound(const ITRSProblem &its, GuardList guard, Expression cost, bool finalCheck);

    void normalizeGuard();
    void createInitialLimitProblem();
    void propagateBounds();
    GiNaC::exmap calcSolution(const LimitProblem &limitProblem);
    int findUpperBoundforSolution(const LimitProblem &limitProblem, const GiNaC::exmap &solution);
    int findLowerBoundforSolvedCost(const LimitProblem &limitProblem, const GiNaC::exmap &solution);
    void removeUnsatProblems();
    bool solveLimitProblem();
    ComplexityResult getComplexity(const LimitProblem &limitProblem);
    bool isAdequateSolution(const LimitProblem &limitProblem);

    void createBacktrackingPoint(const InftyExpressionSet::const_iterator &it, Direction dir);
    bool tryRemovingConstant(const InftyExpressionSet::const_iterator &it);
    bool tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it);
    bool tryReducingPolynomialPower(const InftyExpressionSet::const_iterator &it);
    bool tryReducingGeneralPower(const InftyExpressionSet::const_iterator &it);
    bool tryApplyingLimitVector(const InftyExpressionSet::const_iterator &it);
    bool tryApplyingLimitVectorSmartly(const InftyExpressionSet::const_iterator &it);
    bool tryInstantiatingVariable();
    bool trySubstitutingVariable();

private:
    const ITRSProblem its;
    const GuardList guard;
    const Expression cost;
    bool finalCheck;
    GuardList normalizedGuard;
    ComplexityResult bestComplexity;

    std::vector<LimitProblem> limitProblems;
    std::vector<LimitProblem> solvedLimitProblems;
    LimitProblem currentLP;

    std::vector<GiNaC::exmap> substitutions;

public:
    /**
     * Analyzes the given guard and cost.
     * @param its the ITRSProblem instance is needed to get information about free variables
     * @param finalCheck enables more sophisticated backtracking
     */
    static InfiniteInstances::Result determineComplexity(const ITRSProblem &its,
                                                         const GuardList &guard,
                                                         const Expression &cost, bool finalCheck);
};

#endif //ASYMPTOTICBOUND_H