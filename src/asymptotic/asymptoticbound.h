#ifndef ASYMPTOTICBOUND_H
#define ASYMPTOTICBOUND_H

#include <string>
#include <vector>

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

    //debug dumping
    void dumpCost(const std::string &description) const;
    void dumpGuard(const std::string &description) const;

private:
    const ITRSProblem its;
    const GuardList guard;
    const Expression cost;
    GuardList normalizedGuard;

    LimitProblem limitProblem;
    std::vector<GiNaC::exmap> substitutions;
    GiNaC::exmap solution;
    int upperBound;
    int lowerBound;

public:
    static InfiniteInstances::Result determineComplexity(const ITRSProblem &its, const GuardList &guard, const Expression &cost);
};

#endif //ASYMPTOTICBOUND_H