#include "asymptoticbound.h"

#include "guardtoolbox.h"
#include "asymptotic/limitproblem.h"

using namespace GiNaC;

AsymptoticBound::AsymptoticBound(GuardList guard, Expression cost)
    : guard(guard), cost(cost) {
    assert(GuardToolbox::isValidGuard(guard));
}


void AsymptoticBound::normalizeGuard() {
    debugAsymptoticBound("Normalizing guard.");

    for (Expression &ex : guard) {
        assert(GiNaC::is_a<GiNaC::relational>(ex));

        if (ex.info(info_flags::relation_equal)) {
            // Split equality
            Expression greaterEqual = GuardToolbox::normalize(ex.lhs() >= ex.rhs());
            Expression lessEqual = GuardToolbox::normalize(ex.lhs() <= ex.rhs());

            normalizedGuard.push_back(greaterEqual);
            normalizedGuard.push_back(lessEqual);

            debugAsymptoticBound(ex << " -> " << greaterEqual << " and " << lessEqual);
        } else {
            Expression normalized = GuardToolbox::normalize(ex);

            normalizedGuard.push_back(normalized);

            debugAsymptoticBound(ex << " -> " << normalized);
        }
    }

    debugAsymptoticBound("");
}


void AsymptoticBound::dumpGuard(const std::string &description) const {
#ifdef DEBUG_INFINITY
    std::cout << description << ": ";
    for (auto ex : guard) {
        std::cout << ex << " ";
    }
    std::cout << std::endl;
#endif
}


void AsymptoticBound::determineComplexity(const GuardList &guard, const Expression &cost) {
    AsymptoticBound asymptoticBound(guard, cost);

    debugAsymptoticBound("Analyzing asymptotic bound.");
    asymptoticBound.dumpGuard("guard");
    debugAsymptoticBound("cost: " << cost << std::endl);

    asymptoticBound.normalizeGuard();

    LimitProblem limitProblem(asymptoticBound.normalizedGuard, cost);

    limitProblem.removeConstant(1);

    limitProblem.removePolynomial(1);

    limitProblem.removePolynomial(2);

    limitProblem.applyLimitVector(1, 0, LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_CONS);

    limitProblem.applyLimitVector(1, 0, LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_CONS);

    limitProblem.applyLimitVector(2, 0, LimitProblem::InftyDir::POS_CONS,
                                  LimitProblem::InftyDir::POS_CONS,
                                  LimitProblem::InftyDir::POS_CONS);

    limitProblem.applyLimitVector(3, 0, LimitProblem::InftyDir::POS_CONS,
                                  LimitProblem::InftyDir::POS_CONS,
                                  LimitProblem::InftyDir::POS_CONS);

    limitProblem.removeConstant(3);
    limitProblem.removeConstant(3);
    limitProblem.removeConstant(3);
    limitProblem.removeConstant(3);

    limitProblem.applyLimitVector(1, 0, LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF);

    limitProblem.applyLimitVector(1, 0, LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF);

    limitProblem.applyLimitVector(4, 0, LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF);

    limitProblem.applyLimitVector(5, 0, LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF,
                                  LimitProblem::InftyDir::POS_INF);
}
