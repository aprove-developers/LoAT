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
    void calcSolution();
    void findUpperBoundforSolution();
    void findLowerBoundforSolvedCost();
    bool solveLimitProblem();
    Complexity getComplexity();

    //debug dumping
    void dumpCost(const std::string &description) const;
    void dumpGuard(const std::string &description) const;

private:
    bool tryRemovingConstant(const InftyExpressionSet::const_iterator &it);
    bool tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it);
    bool tryReducingPolynomialPower(const InftyExpressionSet::const_iterator &it);
    bool tryApplyingLimitVector(const InftyExpressionSet::const_iterator &it);

private:
    const ITRSProblem its;
    const GuardList guard;
    const Expression cost;
    GuardList normalizedGuard;

    std::vector<LimitProblem> limitProblems;
    std::vector<GiNaC::exmap> substitutions;
    LimitProblem solvedLimitProblem;
    GiNaC::exmap solution;
    int upperBound;
    int lowerBound;
    bool lowerBoundIsExponential;

public:
    static InfiniteInstances::Result determineComplexity(const ITRSProblem &its, const GuardList &guard, const Expression &cost);
};

#endif //ASYMPTOTICBOUND_H