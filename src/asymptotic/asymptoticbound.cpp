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
    currentLP = LimitProblem(normalizedGuard, cost);
}

void AsymptoticBound::propagateBounds() {
    debugAsymptoticBound("Propagating bounds.");

    assert(substitutions.size() == 0);

    for (const Expression &ex : guard) {
        assert(ex.info(info_flags::relation_equal)
               || GuardToolbox::isValidInequality(ex));

        if (ex.info(info_flags::relation_equal)) {
            Expression target = ex.rhs() - ex.lhs();
            if (!target.info(info_flags::polynomial)) {
                continue;
            }

            //check if equation can be solved for any single variable
            for (const ExprSymbol &var : target.getVariables()) {
                //solve target for var (result is in target)
                if (!GuardToolbox::solveTermFor(target, var, GuardToolbox::PropagationLevel::NoCoefficients)) {
                    continue;
                }

                exmap sub;
                sub.insert(std::make_pair(var, target));
                substitutions.push_back(sub);

                debugAsymptoticBound("substitution: " << sub);
            }
        } else {
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

                if (r.info(info_flags::polynomial) && !r.has(l)) {
                    if (exT.info(info_flags::relation_less) && !swap) { // exT: x = l < r
                        r = r - 1;
                    } else if (exT.info(info_flags::relation_less) && swap) { // exT: r < l = x
                        r = r + 1;
                    }

                    exmap sub;
                    sub.insert(std::make_pair(l, r));
                    substitutions.push_back(sub);

                    debugAsymptoticBound("substitution: " << sub);
                }
            }
        }
    }

    limitProblems.push_back(currentLP);
    limitProblems.back().checkUnsat();

    if (substitutions.size() < sizeof(unsigned int) * 8) {
        unsigned int combination;
        unsigned int to = (1 << substitutions.size()) - 1;

        for (combination = 1; combination < to; combination++) {
            limitProblems.push_back(currentLP);
            LimitProblem &limitProblem = limitProblems.back();

            debugAsymptoticBound("combination of substitutions:");
            for (int bitPos = 0; bitPos < substitutions.size(); ++bitPos) {
                if (combination & (1 << bitPos)) {
                    debugAsymptoticBound(substitutions[bitPos]);
                }
            }

            for (int bitPos = 0; bitPos < substitutions.size(); ++bitPos) {
                if (combination & (1 << bitPos)) {
                    limitProblem.substitute(substitutions[bitPos], bitPos);
                }
            }

            limitProblem.checkUnsat();

            if (limitProblem.isUnsolvable()) {
                limitProblems.pop_back();
            }
        }

    }

    limitProblems.push_back(currentLP);
    LimitProblem &limitProblem = limitProblems.back();

    debugAsymptoticBound("combination of substitutions:");
    for (int i = 0; i < substitutions.size(); ++i) {
        debugAsymptoticBound(substitutions[i]);
    }

    for (int i = 0; i < substitutions.size(); ++i) {
        limitProblem.substitute(substitutions[i], i);
    }

    limitProblem.checkUnsat();

    if (limitProblem.isUnsolvable()) {
        limitProblems.pop_back();
    }
}

GiNaC::exmap AsymptoticBound::calcSolution(const LimitProblem &limitProblem) {
    debugAsymptoticBound("Calculating solution for the initial limit problem.");
    assert(limitProblem.isSolved());

    exmap solution;
    for (int index : limitProblem.getSubstitutions()) {
        const exmap &sub = substitutions[index];

        solution = GuardToolbox::composeSubs(sub, solution);
        debugAsymptoticBound("substitution: " << sub);
    }

    debugAsymptoticBound("solution for the solved limit problem: " << limitProblem.getSolution());
    solution = GuardToolbox::composeSubs(limitProblem.getSolution(), solution);
    debugAsymptoticBound("resulting solution: " << solution << std::endl);

    return solution;
}


int AsymptoticBound::findUpperBoundforSolution(const LimitProblem &limitProblem, const GiNaC::exmap &solution) {
    debugAsymptoticBound("Finding upper bound for the solution.");

    ExprSymbol n = limitProblem.getN();
    int upperBound = 0;
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

    debugAsymptoticBound("O(" << n << "^" << upperBound << ")" << std::endl);

    return upperBound;
}


int AsymptoticBound::findLowerBoundforSolvedCost(const LimitProblem &limitProblem, const GiNaC::exmap &solution) {
    debugAsymptoticBound("Finding lower bound for the solved cost.");

    Expression solvedCost = cost.subs(solution);

    int lowerBound;
    ExprSymbol n = limitProblem.getN();
    if (solvedCost.info(info_flags::polynomial)) {
        assert(solvedCost.is_polynomial(n));
        assert(solvedCost.getVariables().size() <= 1);

        Expression expanded = solvedCost.expand();
        int d = expanded.degree(n);
        debugAsymptoticBound("solved cost: " << expanded << ", degree: " << d);
        lowerBound = d;

        debugAsymptoticBound("Omega(" << n << "^" << lowerBound << ")" << std::endl);

    } else {
        std::vector<Expression> nonPolynomial;
        Expression expanded = solvedCost.expand();
        debugAsymptoticBound("solved cost: " << expanded);

        Expression powerPattern = pow(wild(1), wild(2));
        std::set<ex, ex_is_less> powers;
        assert(expanded.findAll(powerPattern, powers));

        lowerBound = 1;
        for (const Expression &ex : powers) {
            debugAsymptoticBound("power: " << ex);

            if (ex.op(1).has(n) && ex.op(1).is_polynomial(n)) {
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

        debugAsymptoticBound("Omega(" << lowerBound << "^" << n << ")" << std::endl);

        lowerBound = -lowerBound;
    }

    return lowerBound;
}

void AsymptoticBound::removeUnsatProblems() {
    for (int i = limitProblems.size() - 1; i >= 0; --i) {
        if (limitProblems[i].isUnsat()) {
            limitProblems[i].dump("unsat");
            limitProblems.erase(limitProblems.begin() + i);
        }
    }
}


bool AsymptoticBound::solveLimitProblem() {
    debugAsymptoticBound("Trying to solve the initial limit problems.");

    if (limitProblems.empty()) {
        return false;
    }

    currentLP = limitProblems.back();
    limitProblems.pop_back();

    start:
    if (!currentLP.isUnsolvable() && !currentLP.isSolved()) {
        currentLP.dump("Currently handling");

        InftyExpressionSet::const_iterator it;

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryRemovingConstant(it)) {
                goto start;
            }

            if (tryTrimmingPolynomial(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryReducingPolynomialPower(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryReducingGeneralPower(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (it->getVariables().size() <= 1 && tryApplyingLimitVector(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryInstantiatingVariable(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryApplyingLimitVector(it)) {
                goto start;
            }
        }

    }

    if (currentLP.isUnsolvable()) {
        currentLP.dump("Limit problem is unsolvable, throwing away");

    } else if (currentLP.isSolved()) {
        solvedLimitProblems.push_back(currentLP);

        if (isAdequateSolution(currentLP)) {
            return true;
        } else {
            debugAsymptoticBound("Found non-adequate solution.");
        }

    } else {
        currentLP.dump("I don't know how to continue, throwing away");
    }

    if (limitProblems.empty()) {
        return !solvedLimitProblems.empty();

    } else {
        currentLP = limitProblems.back();
        limitProblems.pop_back();
        goto start;
    }
}


Complexity AsymptoticBound::getComplexity(const LimitProblem &limitProblem) {
    GiNaC::exmap solution = calcSolution(limitProblem);
    int upperBound = findUpperBoundforSolution(limitProblem, solution);
    if (upperBound == 0) {
        return Expression::ComplexInfty;
    }

    int lowerBound = findLowerBoundforSolvedCost(limitProblem, solution);

    debugAsymptoticBound("Calculating complexity.");

    ExprSymbol n = limitProblem.getN();
    if (lowerBound < 0) { // lower bound is exponential
        debugAsymptoticBound("Omega(" << -lowerBound << "^"
                             << "(" << n << "^"
                             << "(1/" << upperBound << ")" << ")" << ")" << std::endl);

        return Expression::ComplexExp;
    } else {
        debugAsymptoticBound("Omega(" << n << "^"
                             << "(" << lowerBound << "/" << upperBound << ")" << ")" << std::endl);

        return Complexity(lowerBound, upperBound);
    }
}


Complexity AsymptoticBound::getBestComplexity() {
    Complexity cplx = Expression::ComplexNone;

    for (const LimitProblem &limitProblem : solvedLimitProblems) {
        Complexity newCplx = getComplexity(limitProblem);

        if (newCplx > cplx) {
            cplx = newCplx;
            solutionBestCplx = calcSolution(limitProblem);
            upperBoundBestCplx = findUpperBoundforSolution(limitProblem, solutionBestCplx);
        }
    }

    return cplx;
}


bool AsymptoticBound::isAdequateSolution(const LimitProblem &limitProblem) {
    debugAsymptoticBound("Checking solution for adequateness.");
    assert(limitProblem.isSolved());

    exmap solution = calcSolution(limitProblem);
    Expression solvedCost = cost.subs(solution);
    ExprSymbol n = limitProblem.getN();
    debugAsymptoticBound("solved cost: " << solvedCost << ", cost: " << cost);

    if (solvedCost.is_polynomial(n)) {
        if (!cost.info(info_flags::polynomial)) {
            return false;
        }

        if (cost.getMaxDegree() > solvedCost.degree(n)) {
            return false;
        }

    }

    for (auto pair : solution) {
        assert(is_a<symbol>(pair.first));

        if (its.isFreeVar(ex_to<symbol>(pair.first))
            && is_a<numeric>(pair.second)) {
            return false;
        }
    }

    return true;
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


void AsymptoticBound::createBacktrackingPoint(const InftyExpressionSet::const_iterator &it, InftyDirection dir) {
    assert(dir == POS_INF || dir == POS_CONS);

    if (it->getDirection() == POS) {
        limitProblems.push_back(currentLP);
        limitProblems.back().addExpression(InftyExpression(*it, dir));
    }
}


bool AsymptoticBound::tryRemovingConstant(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.removeConstantIsApplicable(it)) {
        currentLP.removeConstant(it);
        return true;

    } else {
        return false;
    }
}


bool AsymptoticBound::tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.trimPolynomialIsApplicable(it)) {
        //createBacktrackingPoint(it, POS_CONS);

        currentLP.trimPolynomial(it);
        currentLP.checkUnsat();

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingPolynomialPower(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reducePolynomialPowerIsApplicable(it)) {
        //createBacktrackingPoint(it, POS_CONS);

        currentLP.reducePolynomialPower(it);
        currentLP.checkUnsat();

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingGeneralPower(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reduceGeneralPowerIsApplicable(it)) {
        //createBacktrackingPoint(it, POS_CONS);

        currentLP.reduceGeneralPower(it);
        currentLP.checkUnsat();

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryApplyingLimitVector(const InftyExpressionSet::const_iterator &it) {
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

    for (const LimitVector &lv : toApply) {
        if (lv.getType() == POS_INF) {
            //createBacktrackingPoint(it, POS_CONS);
        } else if (lv.getType() == POS_CONS) {
            //createBacktrackingPoint(it, POS_INF);
        }
    }

    if (!toApply.empty()) {
        for (int i = 0; i < toApply.size() - 1; ++i) {
            limitProblems.push_back(currentLP);
            LimitProblem &copy = limitProblems.back();
            auto copyIt = copy.find(*it);

            copy.applyLimitVector(copyIt, 0, toApply[i]);
            copy.checkUnsat();

            if (copy.isUnsolvable()) {
                limitProblems.pop_back();
            }
        }

        currentLP.applyLimitVector(it, 0, toApply.back());
        currentLP.checkUnsat();

        return true;
    } else {
        return false;
    }

}


bool AsymptoticBound::tryInstantiatingVariable(const InftyExpressionSet::const_iterator &it) {
    InftyDirection dir = it->getDirection();
    if (is_a<symbol>(*it) && (dir == POS || dir == POS_CONS || dir == NEG_CONS)) {

        Z3VariableContext context;
        z3::model model(context, Z3_model());
        z3::check_result result;
        result = Z3Toolbox::checkExpressionsSAT(currentLP.getQuery(), context, &model);

        if (result == z3::unsat) {
            currentLP.dump("Z3: limit problem is unsat");
            currentLP.setUnsolvable();

        } else if (result == z3::sat) {
            currentLP.dump("Z3: limit problem is sat");
            Expression rational = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(*it, context));

            exmap sub;
            sub.insert(std::make_pair(*it, rational));
            substitutions.push_back(sub);

            //createBacktrackingPoint(it, POS_INF);
            currentLP.substitute(sub, substitutions.size() - 1);

        } else {
            currentLP.dump("Z3: limit problem is unknown");
            return false;
        }

        return true;
    } else {
        return false;
    }
}


InfiniteInstances::Result AsymptoticBound::determineComplexity(const ITRSProblem &its, const GuardList &guard, const Expression &cost) {
    debugAsymptoticBound("Analyzing asymptotic bound.");

    Expression expandedCost = cost.expand();
    Expression useCost = cost;
    //if cost contains infty, check if coefficient > 0 is SAT, otherwise remove infty symbol
    if (expandedCost.has(Expression::Infty)) {
        Expression inftyCoeff = expandedCost.coeff(Expression::Infty);

        GuardList query = guard;
        query.push_back(inftyCoeff > 0);

        if (Z3Toolbox::checkExpressionsSAT(query) == z3::sat) {
            return InfiniteInstances::Result(Expression::ComplexInfty, false, Expression::Infty, 0, "INF coeff sat");

        }

        useCost = cost.subs(Expression::Infty == 0); //remove INF symbol if INF cost cannot be proved
    }

    AsymptoticBound asymptoticBound(its, guard, cost);
    asymptoticBound.dumpGuard("guard");
    asymptoticBound.dumpCost("cost");
    debugAsymptoticBound("");

    asymptoticBound.normalizeGuard();
    asymptoticBound.createInitialLimitProblem();
    asymptoticBound.propagateBounds();
    asymptoticBound.removeUnsatProblems();

    if (asymptoticBound.solveLimitProblem()) {
        debugAsymptoticBound("Solved the initial limit problem.");

        debugAsymptoticBound(asymptoticBound.solvedLimitProblems.size() << " solved problems");

        return InfiniteInstances::Result(asymptoticBound.getBestComplexity(),
                                         asymptoticBound.upperBoundBestCplx > 1,
                                         asymptoticBound.cost.subs(asymptoticBound.solutionBestCplx),
                                         0, "Solved the initial limit problem.");
    } else {
        debugAsymptoticBound("Could not solve the initial limit problem.");

        return InfiniteInstances::Result(Expression::ComplexNone,
                                         "Could not solve the initial limit problem.");
    }
}
