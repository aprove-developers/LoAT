#include "asymptoticbound.h"

#include <vector>
#include <ginac/ginac.h>

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

    InftyExpressionSet::const_iterator it;
    std::vector<InftyExpressionSet::const_iterator> its;

    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (it->info(info_flags::integer)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.removeConstant(i);
    }

    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (it->info(info_flags::polynomial)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.trimPolynomial(i);
    }

    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (it->info(info_flags::polynomial)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.trimPolynomial(i);
    }


    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {

        ExprSymbolSet variables = it->getVariables();

        ExprSymbol var = *variables.begin();
        if (it->info(info_flags::polynomial) && !(it->lcoeff(var).info(info_flags::integer))) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.applyLimitVector(i, 0, InftyDirection::POS_INF,
                                  InftyDirection::POS_INF,
                                  InftyDirection::POS_CONS);
    }


    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (it->info(info_flags::rational)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.applyLimitVector(i, 0, InftyDirection::POS_CONS,
                                  InftyDirection::POS_CONS,
                                  InftyDirection::POS_CONS);
    }

    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (it->info(info_flags::integer)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.removeConstant(i);
    }


    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {

        ExprSymbolSet variables = it->getVariables();

        ExprSymbol var = *variables.begin();
        if (is_a<power>(*it) && (it->op(1) - 1).info(info_flags::positive)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.applyLimitVector(i, 0, InftyDirection::POS_INF,
                                  InftyDirection::POS_INF,
                                  InftyDirection::POS_INF);
    }


    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {

        ExprSymbolSet variables = it->getVariables();

        ExprSymbol var = *variables.begin();
        if (is_a<power>(*it) && (it->op(1) - 1).info(info_flags::positive)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.applyLimitVector(i, 0, InftyDirection::POS_INF,
                                  InftyDirection::POS_INF,
                                  InftyDirection::POS_INF);
    }


    its.clear();
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {

        ExprSymbolSet variables = it->getVariables();

        ExprSymbol var = *variables.begin();
        if (is_a<power>(*it) && (it->op(1) - 1).info(info_flags::positive)) {
            its.push_back(it);
        }
    }

    for (auto i : its) {
        limitProblem.applyLimitVector(i, 0, InftyDirection::POS_INF,
                                  InftyDirection::POS_INF,
                                  InftyDirection::POS_INF);
    }

}
