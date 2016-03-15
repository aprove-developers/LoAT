#include "asymptoticbound.h"

#include <vector>
#include <ginac/ginac.h>

#include "expression.h"
#include "guardtoolbox.h"

using namespace GiNaC;

AsymptoticBound::AsymptoticBound(const ITRSProblem &its, GuardList guard, Expression cost)
    : its(its), guard(guard), cost(cost) {
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

        if (!its.isFreeVar(ex_to<symbol>(pair.first))) {
            Expression sub = pair.second;
            assert(sub.is_polynomial(n));
            assert(sub.getVariables().size() <= 1);

            Expression expanded = sub.expand();
            int d = expanded.degree(n);
            debugAsymptoticBound(pair.first << "==" << expanded << ", degree: " << d);

            if (d > upperBound) {
                upperBound = d;
            }
        } else {
            debugAsymptoticBound(pair.first << " is not a program variable");
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
        lowerBoundIsExponential = false;

        debugAsymptoticBound("Omega(" << n << "^" << lowerBound << ")" << std::endl);

    } else {
        std::vector<Expression> nonPolynomial;
        Expression expanded = solvedCost.expand();
        debugAsymptoticBound("solved cost: " << expanded);

        Expression powerPattern = pow(wild(1), wild(2));
        std::set<ex, ex_is_less> powers;
        assert(expanded.find(powerPattern, powers));

        lowerBound = 1;
        for (const Expression &ex : powers) {
            if (ex.op(1).has(n)) {
                debugAsymptoticBound("power: " << ex);
                assert(ex.op(1).is_polynomial(n));
                assert(ex.op(0).info(info_flags::integer));
                assert(ex.op(0).info(info_flags::positive));

                int base =  ex_to<numeric>(ex.op(0)).to_int();
                debugAsymptoticBound("base: " << base);
                if (base > lowerBound) {
                    lowerBound = base;
                }
            }
        }
        assert(lowerBound > 1);
        lowerBoundIsExponential = true;

        debugAsymptoticBound("Omega(" << lowerBound << "^" << n << ")" << std::endl);
    }
}


bool AsymptoticBound::solveLimitProblem() {
    InftyExpressionSet::const_iterator it;

    start:
    // Highest priority
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (limitProblem.removeConstantIsApplicable(it)) {
            limitProblem.removeConstant(it);
            goto start;
        }

        if (limitProblem.trimPolynomialIsApplicable(it)) {
            limitProblem.trimPolynomial(it);
            goto start;
        }
    }

    // Second highest priority
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (limitProblem.reducePolynomialPowerIsApplicable(it)) {
            limitProblem.reducePolynomialPower(it);
            goto start;
        }
    }

    // Third highest priority
    for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
        if (it->getVariables().size() == 1) {
            if (it->isProperRational()) {
                for (const LimitVector &lv : LimitVector::Division) {
                    if (lv.isApplicable(it->getDirection())) {
                        limitProblem.applyLimitVector(it, 0, lv);
                        goto start;
                    }
                }
            } else if (is_a<add>(*it)) {
                for (const LimitVector &lv : LimitVector::Addition) {
                    if (lv.isApplicable(it->getDirection())) {
                        limitProblem.applyLimitVector(it, 0, lv);
                        goto start;
                    }
                }
            } else if (is_a<mul>(*it)) {
                for (const LimitVector &lv : LimitVector::Multiplication) {
                    if (lv.isApplicable(it->getDirection())) {
                        limitProblem.applyLimitVector(it, 0, lv);
                        goto start;
                    }
                }
            } else if (it->isProperNaturalPower()) {
                for (const LimitVector &lv : LimitVector::Multiplication) {
                    if (lv.isApplicable(it->getDirection())) {
                        limitProblem.applyLimitVector(it, 0, lv);
                        goto start;
                    }
                }
            }
        }
    }

    return limitProblem.isSolved();
}


Complexity AsymptoticBound::getComplexity() {
    debugAsymptoticBound("Calculating complexity.");

    ExprSymbol n = limitProblem.getN();
    if (lowerBoundIsExponential) {
        debugAsymptoticBound("Omega(" << lowerBound << "^"
                             << "(" << n << "^"
                             << "(1/" << upperBound << ")" << ")" << ")" << std::endl);

        return Expression::ComplexExp;
    } else {
        debugAsymptoticBound("Omega(" << n << "^"
                             << "(" << lowerBound << "/" << upperBound << ")" << ")" << std::endl);

        return Complexity(lowerBound, upperBound);
    }
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


InfiniteInstances::Result AsymptoticBound::determineComplexity(const ITRSProblem &its, const GuardList &guard, const Expression &cost) {
    debugAsymptoticBound("Analyzing asymptotic bound.");

    AsymptoticBound asymptoticBound(its, guard, cost);
    asymptoticBound.dumpGuard("guard");
    asymptoticBound.dumpCost("cost");
    debugAsymptoticBound("");

    asymptoticBound.normalizeGuard();
    asymptoticBound.createInitialLimitProblem();
    asymptoticBound.propagateBounds();

    if (asymptoticBound.solveLimitProblem()) {
        debugAsymptoticBound("Solved the initial limit problem.");

        asymptoticBound.calcSolution();
        asymptoticBound.findUpperBoundforSolution();
        asymptoticBound.findLowerBoundforSolvedCost();

        return InfiniteInstances::Result(asymptoticBound.getComplexity(),
                                         asymptoticBound.upperBound > 1,
                                         asymptoticBound.cost.subs(asymptoticBound.solution),
                                         0, "Solved the initial limit problem.");
    } else {
        debugAsymptoticBound("Could not solve the initial limit problem.");

        return InfiniteInstances::Result(Expression::ComplexNone,
                                         "Could not solve the initial limit problem.");
    }
}
