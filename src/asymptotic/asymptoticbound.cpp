#include "asymptoticbound.h"

#include <vector>
#include <ginac/ginac.h>

#include "expression.h"
#include "guardtoolbox.h"

using namespace GiNaC;

AsymptoticBound::AsymptoticBound(GuardList guard, Expression cost)
    : guard(guard), cost(cost) {
    assert(GuardToolbox::isValidGuard(guard));
}


void AsymptoticBound::normalizeGuard() {
    debugAsymptoticBound("Normalizing guard.");

    for (const Expression &ex : guard) {
        assert(ex.info(info_flags::relation_equal)
               || GuardToolbox::isValidInequality(ex));

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


void AsymptoticBound::createInitialLimitProblem() {
    limitProblem = LimitProblem(normalizedGuard, cost);
}


void AsymptoticBound::propagateBounds() {
    // TODO: Improve
    debugAsymptoticBound("Propagating bounds.");

    for (const Expression &ex : guard) {
        assert(ex.info(info_flags::relation_equal)
               || GuardToolbox::isValidInequality(ex));

        if (is_a<symbol>(ex.lhs()) || is_a<symbol>(ex.rhs())) {
            Expression exT = GuardToolbox::turnToLess(ex);

            Expression l, r;
            bool swap = is_a<symbol>(exT.rhs());
            if (swap) {
                l = exT.rhs();
                r = exT.lhs();
            } else {
                l = exT.lhs();
                r = exT.rhs();
            }

            if (!r.info(info_flags::polynomial)) {
                continue;
            }


            if (!r.has(l) && !(!exT.info(info_flags::relation_equal) && is_a<numeric>(r))) {
                if (exT.info(info_flags::relation_less) && !swap) { // exT: x = l < r
                    r = r - 1;
                } else if (exT.info(info_flags::relation_less) && swap) { // exT: r < l = x
                    r = r + 1;
                }

                exmap sub;
                sub.insert(std::make_pair(l, r));

                limitProblem.substitute(sub);

                substitutions.push_back(sub);
            }
        }
    }
}

void AsymptoticBound::calcSolution() {
    debugAsymptoticBound("Calculating solution for the initial limit problem.");
    assert(limitProblem.isSolved());


    solution.clear();
    for (const exmap &sub : substitutions) {
        solution = GuardToolbox::composeSubs(sub, solution);
        debugAsymptoticBound("substitution: " << sub);
    }

    debugAsymptoticBound("solution for the solved limit problem: " << limitProblem.getSolution());
    solution = GuardToolbox::composeSubs(limitProblem.getSolution(), solution);
    debugAsymptoticBound("resulting solution: " << solution << std::endl);
}


void AsymptoticBound::findUpperBoundforSolution() {
    debugAsymptoticBound("Finding upper bound for the solution.");

    ExprSymbol n = limitProblem.getN();
    upperBound = 0;
    for (auto pair : solution) {
        assert(is_a<symbol>(pair.first));
        Expression sub = pair.second;
        assert(sub.is_polynomial(n));
        assert(sub.getVariables().size() <= 1);

        Expression expanded = sub.expand();
        int d = expanded.degree(n);
        debugAsymptoticBound(pair.first << "==" << expanded << ", degree: " << d);

        if (d > upperBound) {
            upperBound = d;
        }
    }
    assert(upperBound > 0);

    debugAsymptoticBound("O(" << n << "^" << upperBound << ")" << std::endl);
}


void AsymptoticBound::findLowerBoundforSolvedCost() {
    debugAsymptoticBound("Finding lower bound for the solved cost.");

    Expression solvedCost = cost.subs(solution);

    ExprSymbol n = limitProblem.getN();
    if (solvedCost.info(info_flags::polynomial)) {
        assert(solvedCost.is_polynomial(n));
        assert(solvedCost.getVariables().size() <= 1);

        Expression expanded = solvedCost.expand();
        int d = expanded.degree(n);
        debugAsymptoticBound("solved cost: " << expanded << ", degree: " << d);
        lowerBound = d;

    } else {
        assert(false); // TODO
    }

    debugAsymptoticBound("Omega(" << n << "^" << lowerBound << ")" << std::endl);
}


void AsymptoticBound::dumpCost(const std::string &description) const {
    debugAsymptoticBound(description << ": " << cost);
}


void AsymptoticBound::dumpGuard(const std::string &description) const {
#ifdef DEBUG_ASYMPTOTIC_BOUNDS
    std::cout << description << ": ";
    for (auto ex : guard) {
        std::cout << ex << " ";
    }
    std::cout << std::endl;
#endif
}


InfiniteInstances::Result AsymptoticBound::determineComplexity(const GuardList &guard, const Expression &cost) {
    debugAsymptoticBound("Analyzing asymptotic bound.");

    AsymptoticBound asymptoticBound(guard, cost);
    asymptoticBound.dumpGuard("guard");
    asymptoticBound.dumpCost("cost");
    debugAsymptoticBound("");

    asymptoticBound.normalizeGuard();
    asymptoticBound.createInitialLimitProblem();
    asymptoticBound.propagateBounds();

    LimitProblem &limitProblem = asymptoticBound.limitProblem;
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

    /*its.clear();
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
    }*/

    asymptoticBound.calcSolution();
    asymptoticBound.findUpperBoundforSolution();
    asymptoticBound.findLowerBoundforSolvedCost();

    Complexity cplx(asymptoticBound.lowerBound, asymptoticBound.upperBound);

    return InfiniteInstances::Result(cplx, asymptoticBound.upperBound > 1,
                                     asymptoticBound.cost.subs(asymptoticBound.solution), 0,
                                     "Solved the initial limit problem.");
}
