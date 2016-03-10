#ifndef ASYMPTOTICBOUND_H
#define ASYMPTOTICBOUND_H

#include <string>

#include "guardtoolbox.h"

class AsymptoticBound {
public:
    AsymptoticBound(GuardList guard, Expression cost);

    void normalizeGuard();

private:
    GuardList guard;
    GuardList normalizedGuard;
    Expression cost;

private:
    //debug dumping
    void dumpGuard(const std::string &description) const;

public:
    static void determineComplexity(const GuardList &guard, const Expression &cost);
};

#endif //ASYMPTOTICBOUND_H