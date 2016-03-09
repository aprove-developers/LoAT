#include "asymptoticbound.h"

AsymptoticBound::AsymptoticBound(GuardList guard, Expression cost)
    : guard(guard), cost(cost) {
}

void AsymptoticBound::determineComplexity(GuardList guard, Expression cost) {
    AsymptoticBound asymptoticBound(guard, cost);

    debugAsymptoticBound("Analyzing asymptotic bound.");
    asymptoticBound.dumpGuard("guard");
    debugAsymptoticBound("cost: " << cost);
}

void AsymptoticBound::dumpGuard(const std::string &description) const {
#ifdef DEBUG_INFINITY
    std::cout << description << ": ";
    for (auto ex : guard) std::cout << ex << " ";
    std::cout << "| " << cost;
    std::cout << std::endl;
#endif
}