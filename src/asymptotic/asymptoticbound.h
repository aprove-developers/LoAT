#ifndef ASYMPTOTICBOUND_H
#define ASYMPTOTICBOUND_H

#include <string>
#include <vector>

#include "expression.h"
#include "guardtoolbox.h"
#include "infinity.h"
#include "itrs.h"
#include "asymptotic/limitproblem.h"

class AsymptoticBound {
private:
    AsymptoticBound(const ITRSProblem &its, GuardList guard, Expression cost);

    void normalizeGuard();
    void createInitialLimitProblem();
    void propagateBounds();
    GiNaC::exmap calcSolution(const LimitProblem &limitProblem);
    int findUpperBoundforSolution(const LimitProblem &limitProblem, const GiNaC::exmap &solution);
    int findLowerBoundforSolvedCost(const LimitProblem &limitProblem, const GiNaC::exmap &solution);
    void removeUnsatProblems();
    bool solveLimitProblem();
    Complexity getComplexity(const LimitProblem &limitProblem);
    Complexity getBestComplexity();
    bool isAdequateSolution(const LimitProblem &limitProblem);

    //debug dumping
    void dumpCost(const std::string &description) const;
    void dumpGuard(const std::string &description) const;

private:
    void createBacktrackingPoint(const InftyExpressionSet::const_iterator &it, InftyDirection dir);
    bool tryRemovingConstant(const InftyExpressionSet::const_iterator &it);
    bool tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it);
    bool tryReducingPolynomialPower(const InftyExpressionSet::const_iterator &it);
    bool tryReducingGeneralPower(const InftyExpressionSet::const_iterator &it);
    bool tryApplyingLimitVector(const InftyExpressionSet::const_iterator &it);
    bool tryInstantiatingVariable(const InftyExpressionSet::const_iterator &it);

private:
    const ITRSProblem its;
    const GuardList guard;
    const Expression cost;
    GuardList normalizedGuard;

    std::vector<LimitProblem> limitProblems;
    std::vector<LimitProblem> solvedLimitProblems;
    LimitProblem currentLP;
    std::vector<GiNaC::exmap> substitutions;
    GiNaC::exmap solutionBestCplx;
    int upperBoundBestCplx;

public:
    static InfiniteInstances::Result determineComplexity(const ITRSProblem &its, const GuardList &guard, const Expression &cost);
};

#endif //ASYMPTOTICBOUND_H