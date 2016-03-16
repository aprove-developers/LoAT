#include "asymptoticbound.h"

#include <vector>
#include <ginac/ginac.h>
#include <z3++.h>

#include "expression.h"
#include "guardtoolbox.h"
#include "z3toolbox.h"

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
    limitProblems.push_back(LimitProblem(normalizedGuard, cost));
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
                substitutions.push_back(sub);

                limitProblems.begin()->substitute(sub, substitutions.size() - 1);
            }
        }
    }
}

void AsymptoticBound::calcSolution() {
    debugAsymptoticBound("Calculating solution for the initial limit problem.");
    assert(solvedLimitProblem.isSolved());


    solution.clear();
    for (int index : solvedLimitProblem.getSubstitutions()) {
        const exmap &sub = substitutions[index];

        solution = GuardToolbox::composeSubs(sub, solution);
        debugAsymptoticBound("substitution: " << sub);
    }

    debugAsymptoticBound("solution for the solved limit problem: " << solvedLimitProblem.getSolution());
    solution = GuardToolbox::composeSubs(solvedLimitProblem.getSolution(), solution);
    debugAsymptoticBound("resulting solution: " << solution << std::endl);
}


void AsymptoticBound::findUpperBoundforSolution() {
    debugAsymptoticBound("Finding upper bound for the solution.");

    ExprSymbol n = solvedLimitProblem.getN();
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

    ExprSymbol n = solvedLimitProblem.getN();
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
    if (limitProblems.size() > 0 && !limitProblems.back().isSolved()) {
        LimitProblem &limitProblem = limitProblems.back();
        limitProblem.dump("Currently handling");

        // Highest priority
        for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
            if (tryRemovingConstant(it)) {
                goto start;
            }

            if (tryTrimmingPolynomial(it)) {
                goto start;
            }
        }

        // Second highest priority
        for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
            if (tryReducingPolynomialPower(it)) {
                goto start;
            }
        }

        // Third highest priority
        for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
            if (it->getVariables().size() <= 1 && tryApplyingLimitVector(it)) {
                goto start;
            }
        }

        // Fourth highest priority
        for (it = limitProblem.cbegin(); it != limitProblem.cend(); ++it) {
            if (tryInstantiatingVariable(it)) {
                goto start;
            }
        }

    }

    if (limitProblems.size() > 0 && !limitProblems.back().isSolved()) {
        limitProblems.back().dump("I don't know how to continue, throwing away");
        limitProblems.pop_back();
        goto start;
    }

    if (limitProblems.size() == 0) {
        return false;
    }

    solvedLimitProblem = limitProblems.back();

    return solvedLimitProblem.isSolved();
}


Complexity AsymptoticBound::getComplexity() {
    debugAsymptoticBound("Calculating complexity.");

    ExprSymbol n = solvedLimitProblem.getN();
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


bool AsymptoticBound::tryRemovingConstant(const InftyExpressionSet::const_iterator &it) {
    LimitProblem &limitProblem = limitProblems.back();

    if (limitProblem.removeConstantIsApplicable(it)) {
        try {
            limitProblem.removeConstant(it);
        } catch (const LimitProblemIsContradictoryException &e) {
            debugAsymptoticBound(e.what());
            limitProblems.pop_back();
        }

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it) {
    LimitProblem &limitProblem = limitProblems.back();

    if (limitProblem.trimPolynomialIsApplicable(it)) {
        try {
            limitProblem.trimPolynomial(it);
        } catch (const LimitProblemIsContradictoryException &e) {
            debugAsymptoticBound(e.what());
            limitProblems.pop_back();
        }

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingPolynomialPower(const InftyExpressionSet::const_iterator &it) {
    LimitProblem &limitProblem = limitProblems.back();

    if (limitProblem.reducePolynomialPowerIsApplicable(it)) {
        try {
            limitProblem.reducePolynomialPower(it);
        } catch (const LimitProblemIsContradictoryException &e) {
            debugAsymptoticBound(e.what());
            limitProblems.pop_back();
        }

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryApplyingLimitVector(const InftyExpressionSet::const_iterator &it) {
    LimitProblem &limitProblem = limitProblems.back();

    std::vector<LimitVector> toApply;

    if (it->isProperRational()) {
        for (const LimitVector &lv : LimitVector::Division) {
            if (lv.isApplicable(it->getDirection())) {
                toApply.push_back(lv);
            }
        }
    } else if (is_a<add>(*it)) {
        for (const LimitVector &lv : LimitVector::Addition) {
            if (lv.isApplicable(it->getDirection())) {
                toApply.push_back(lv);
            }
        }
    } else if (is_a<mul>(*it)) {
        for (const LimitVector &lv : LimitVector::Multiplication) {
            if (lv.isApplicable(it->getDirection())) {
                toApply.push_back(lv);
            }
        }
    } else if (it->isProperNaturalPower()) {
        for (const LimitVector &lv : LimitVector::Multiplication) {
            if (lv.isApplicable(it->getDirection())) {
                toApply.push_back(lv);
            }
        }
    }

    it->dump("expression");
    debugAsymptoticBound("applicable limit vectors:");
    for (const LimitVector &lv : toApply) {
        debugAsymptoticBound(lv);
    }
    debugAsymptoticBound("");

    if (toApply.size() > 0) {
        if (toApply.size() == 1) {
            try {
                limitProblem.applyLimitVector(it, 0, toApply.front());
            } catch (const LimitProblemIsContradictoryException &e) {
                debugAsymptoticBound(e.what());
                limitProblems.pop_back();
            }

        } else {
            LimitProblem copy = limitProblem;
            InftyExpressionSet::const_iterator copyIt = copy.find(*it);
            limitProblems.pop_back();

            for (const LimitVector &lv : toApply) {
                limitProblems.push_back(copy);
                InftyExpressionSet::const_iterator backIt = limitProblems.back().find(*copyIt);

                try {
                    limitProblems.back().applyLimitVector(backIt, 0, lv);
                } catch (const LimitProblemIsContradictoryException &e) {
                    debugAsymptoticBound(e.what());
                    limitProblems.pop_back();
                }

            }
        }

        return true;
    } else {
        return false;
    }

}


bool AsymptoticBound::tryInstantiatingVariable(const InftyExpressionSet::const_iterator &it) {
    LimitProblem &limitProblem = limitProblems.back();

    InftyDirection dir = it->getDirection();
    if (is_a<symbol>(*it) && (dir == POS || dir == POS_CONS || dir == NEG_CONS)) {
        std::vector<Expression> query;

        InftyExpressionSet::const_iterator i;
        for (i = limitProblem.cbegin(); i != limitProblem.cend(); ++i) {
            if (i->getDirection() == NEG_INF || i->getDirection() == NEG_CONS) {
                query.push_back(*i < 0);
            } else {
                query.push_back(*i > 0);
            }
        }

        Z3VariableContext context;
        z3::model model(context,Z3_model());
        z3::check_result result;
        result = Z3Toolbox::checkExpressionsSAT(query, context, &model);

        if (result == z3::unsat) {
            limitProblem.dump("Z3: limit problem is unsat, throwing away");
            limitProblems.pop_back();

        } else if (result == z3::sat) {
            limitProblem.dump("Z3: limit problem is sat");
            Expression rational = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(*it, context));

            exmap sub;
            sub.insert(std::make_pair(*it, rational));
            substitutions.push_back(sub);

            try {
                limitProblem.substitute(sub, substitutions.size() - 1);
            } catch (const LimitProblemIsContradictoryException &e) {
                debugAsymptoticBound(e.what());
                limitProblems.pop_back();
            }

        } else {
            limitProblem.dump("Z3: limit problem is unknown");
            return false;
        }

        return true;
    } else {
        return false;
    }
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
