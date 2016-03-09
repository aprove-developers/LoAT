#ifndef ASYMPTOTICBOUND_H
#define ASYMPTOTICBOUND_H

#include <string>

#include "guardtoolbox.h"

class AsymptoticBound {
public:
    AsymptoticBound(GuardList guard, Expression cost);

    static void determineComplexity(GuardList guard, Expression cost);
private:
    GuardList guard;
    Expression cost;

private:
    //debug dumping
    void dumpGuard(const std::string &description) const;

};

#endif //ASYMPTOTICBOUND_H