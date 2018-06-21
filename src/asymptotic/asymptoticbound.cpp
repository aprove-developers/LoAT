#include "asymptoticbound.h"

#include <iterator>
#include <vector>
#include <ginac/ginac.h>
#include <z3++.h>

#include "expr/expression.h"
#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "expr/ginactoz3.h"
#include "util/timeout.h"

#include "z3/z3solver.h"
#include "z3/z3context.h"
#include "z3/z3toolbox.h"

#include "limitsmt.h"
#include "inftyexpression.h"
#include "global.h"

using namespace GiNaC;
using namespace std;


AsymptoticBound::AsymptoticBound(const VarMan &varMan, GuardList guard,
                                 Expression cost, bool finalCheck)
    : varMan(varMan), guard(guard), cost(cost), finalCheck(finalCheck),
      addition(DirectionSize), multiplication(DirectionSize), division(DirectionSize) {
    assert(GuardToolbox::isWellformedGuard(guard));
}


void AsymptoticBound::initLimitVectors() {
    debugAsymptoticBound("Initializing limit vectors.");

    for (int i = 0; i < DirectionSize; ++i) {
        Direction dir = static_cast<Direction>(i);
        debugAsymptoticBound("Direction: " << DirectionNames[dir]);

        debugAsymptoticBound("Addition:");
        for (const LimitVector &lv : LimitVector::Addition) {
            if (lv.isApplicable(dir)) {
                debugAsymptoticBound(lv);
                addition[dir].push_back(lv);
            }
        }

        debugAsymptoticBound("Multiplication:");
        for (const LimitVector &lv : LimitVector::Multiplication) {
            if (lv.isApplicable(dir)) {
                debugAsymptoticBound(lv);
                multiplication[dir].push_back(lv);
            }
        }

        debugAsymptoticBound("Division:");
        for (const LimitVector &lv : LimitVector::Division) {
            if (lv.isApplicable(dir)) {
                debugAsymptoticBound(lv);
                division[dir].push_back(lv);
            }
        }
    }

    debugAsymptoticBound("");
}


void AsymptoticBound::normalizeGuard() {
    debugAsymptoticBound("Normalizing guard.");

    for (const Expression &ex : guard) {
        assert(Relation::isRelation(ex));

        if (ex.info(info_flags::relation_equal)) {
            // Split equation
            Expression greaterEqual = Relation::normalizeInequality(ex.lhs() >= ex.rhs());
            Expression lessEqual = Relation::normalizeInequality(ex.lhs() <= ex.rhs());

            normalizedGuard.push_back(greaterEqual);
            normalizedGuard.push_back(lessEqual);

            debugAsymptoticBound(ex << " -> " << greaterEqual << " and " << lessEqual);
        } else {
            Expression normalized = Relation::normalizeInequality(ex);

            normalizedGuard.push_back(normalized);

            debugAsymptoticBound(ex << " -> " << normalized);
        }
    }

    debugAsymptoticBound("");
}

void AsymptoticBound::createInitialLimitProblem() {
    debugLimitProblem("Creating initial limit problem.");
    currentLP = LimitProblem(normalizedGuard, cost);

    debugLimitProblem(currentLP);
    debugLimitProblem("");
}

void AsymptoticBound::propagateBounds() {
    debugAsymptoticBound("Propagating bounds.");
    assert(substitutions.size() == 0);

    if (currentLP.isUnsolvable()) {
        return;
    }

    // build substitutions from equations
    for (const Expression &ex : guard) {
        assert(Relation::isRelation(ex));

        Expression target = ex.rhs() - ex.lhs();
        if (ex.info(info_flags::relation_equal)
            && target.info(info_flags::polynomial)) {

            std::vector<ExprSymbol> vars;
            std::vector<ExprSymbol> tempVars;
            for (const ExprSymbol & var : target.getVariables()) {
                if (varMan.isTempVar(var)) {
                    tempVars.push_back(var);
                } else {
                    vars.push_back(var);
                }
            }

            vars.insert(vars.begin(), tempVars.begin(), tempVars.end());

            // check if equation can be solved for a single variable
            // check program variables first
            for (const ExprSymbol &var : vars) {
                //solve target for var (result is in target)
                auto optSolved = GuardToolbox::solveTermFor(target, var, GuardToolbox::TrivialCoeffs);
                if (optSolved) {
                    exmap sub;
                    sub.insert(std::make_pair(var, optSolved.get()));

                    debugAsymptoticBound("substitution (equation): " << sub);
                    substitutions.push_back(std::move(sub));
                    break;
                }
            }
        }
    }

    // apply all substitutions resulting from equations
    for (int i = 0; i < substitutions.size(); ++i) {
        currentLP.substitute(substitutions[i], i);
    }

    if (currentLP.isUnsolvable()) {
        return;
    }

    int numOfEquations = substitutions.size();

    // build substitutions from inequalities
    for (const Expression &ex : guard) {
        if (!ex.info(info_flags::relation_equal)) {
            if (is_a<symbol>(ex.lhs()) || is_a<symbol>(ex.rhs())) {
                Expression exT = Relation::toLessOrLessEq(ex);

                Expression l, r;
                bool swap = is_a<symbol>(exT.rhs());
                if (swap) {
                    l = exT.rhs();
                    r = exT.lhs();
                } else {
                    l = exT.lhs();
                    r = exT.rhs();
                }

                bool isInLimitProblem = false;
                for (auto it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
                    if (it->has(l)) {
                        isInLimitProblem = true;
                    }
                }

                if (!isInLimitProblem) {
                    debugAsymptoticBound(l << " is not in the limit problem");
                    continue;
                }

                if (r.info(info_flags::polynomial) && !r.has(l)) {
                    if (exT.info(info_flags::relation_less) && !swap) { // exT: x = l < r
                        r = r - 1;
                    } else if (exT.info(info_flags::relation_less) && swap) { // exT: r < l = x
                        r = r + 1;
                    }

                    exmap sub;
                    sub.insert(std::make_pair(l, r));

                    debugAsymptoticBound("substitution (inequality): " << sub);
                    substitutions.push_back(std::move(sub));
                }
            }
        }
    }

    // build all possible combinations of substitutions (resulting from inequalites)
    int numOfSubstitutions = substitutions.size() - numOfEquations;
    if (finalCheck && numOfSubstitutions <= 10) { // must be smaller than 32
        unsigned int combination;
        unsigned int to = (1 << numOfSubstitutions) - 1;

        for (combination = 1; combination < to; combination++) {
            limitProblems.push_back(currentLP);
            LimitProblem &limitProblem = limitProblems.back();

            debugAsymptoticBound("combination of substitutions:");
            for (int bitPos = 0; bitPos < numOfSubstitutions; ++bitPos) {
                if (combination & (1 << bitPos)) {
                    debugAsymptoticBound(substitutions[numOfEquations + bitPos]);
                }
            }

            for (int bitPos = 0; bitPos < numOfSubstitutions; ++bitPos) {
                if (combination & (1 << bitPos)) {
                    limitProblem.substitute(substitutions[numOfEquations + bitPos],
                                            numOfEquations + bitPos);
                }
            }

            if (limitProblem.isUnsolvable()) {
                limitProblems.pop_back();
            }
        }

    }

    // no substitution (resulting from inequalies)
    limitProblems.push_back(currentLP);

    if (limitProblems.back().isUnsolvable()) {
        limitProblems.pop_back();
    }

    // all substitutions (resulting from inequalies)
    limitProblems.push_back(currentLP);
    LimitProblem &limitProblem = limitProblems.back();

    debugAsymptoticBound("combination of substitutions:");
    for (int i = numOfEquations; i < substitutions.size(); ++i) {
        debugAsymptoticBound(substitutions[i]);
    }

    for (int i = numOfEquations; i < substitutions.size(); ++i) {
        limitProblem.substitute(substitutions[i], i);
    }

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

        solution = std::move(GuardToolbox::composeSubs(sub, solution));
        debugAsymptoticBound("substitution: " << sub);
    }

    debugAsymptoticBound("solution for the solved limit problem: " << limitProblem.getSolution());
    solution = std::move(GuardToolbox::composeSubs(limitProblem.getSolution(), solution));
    debugAsymptoticBound("resulting solution: " << solution);

    debugAsymptoticBound("fixing solution");

    GuardList guardCopy = guard;
    guardCopy.push_back(cost);
    for (const Expression &ex : guardCopy) {
        for (const ExprSymbol &var : ex.getVariables()) {
            if (solution.count(var) == 0) {
                debugAsymptoticBound(var << " is missing");

                exmap sub;
                sub.insert(std::make_pair(var, numeric(0)));

                solution = std::move(GuardToolbox::composeSubs(sub, solution));
            }
        }
    }

    debugAsymptoticBound("fixed solution: " << solution << std::endl);

    return solution;
}


int AsymptoticBound::findUpperBoundforSolution(const LimitProblem &limitProblem,
                                               const GiNaC::exmap &solution) {
    debugAsymptoticBound("Finding upper bound for the solution.");

    ExprSymbol n = limitProblem.getN();
    int upperBound = 0;
    for (auto const &pair : solution) {
        assert(is_a<symbol>(pair.first));

        if (!varMan.isTempVar(ex_to<symbol>(pair.first))) {
            Expression sub = pair.second;
            assert(sub.is_polynomial(n));
            assert(sub.hasNoVariables()
                   || (sub.hasExactlyOneVariable() && sub.has(n)));

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


int AsymptoticBound::findLowerBoundforSolvedCost(const LimitProblem &limitProblem,
                                                 const GiNaC::exmap &solution) {
    debugAsymptoticBound("Finding lower bound for the solved cost.");

    Expression solvedCost = cost.subs(solution);

    debugAsymptoticBound("COST:  " << cost << "  ===>  " << solvedCost);

    int lowerBound;
    ExprSymbol n = limitProblem.getN();
    if (solvedCost.info(info_flags::polynomial)) {
        assert(solvedCost.is_polynomial(n));
        assert(solvedCost.hasAtMostOneVariable());

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

        // code the lowerBound as a negative number to mark it as exponential
        lowerBound = -lowerBound;
    }

    return lowerBound;
}

void AsymptoticBound::removeUnsatProblems() {
    for (int i = limitProblems.size() - 1; i >= 0; --i) {
        auto result = Z3Toolbox::checkAll(limitProblems[i].getQuery());

        if (result == z3::unsat) {
            debugAsymptoticBound("unsat:");
            debugAsymptoticBound(limitProblems[i]);
            limitProblems.erase(limitProblems.begin() + i);
        } else if (result == z3::unknown
                   && !finalCheck
                   && limitProblems[i].getSize() >= Config::Limit::ProblemDiscardSize) {
            debugAsymptoticBound("removing a limit problem since this is not the final check"
                                 << " and it is very large (" << limitProblems[i].getSize()
                                 << " InftyExpressions)");
            limitProblems.erase(limitProblems.begin() + i);
        }
    }
}


bool AsymptoticBound::solveLimitProblem() {
    debugAsymptoticBound("Trying to solve the initial limit problem.");

    if (limitProblems.empty()) {
        return false;
    }

    bool smtApplicable = Config::Limit::UseSmtEncoding && LimitSmtEncoding::isApplicable(cost);

    currentLP = std::move(limitProblems.back());
    limitProblems.pop_back();

    start:
    if (!currentLP.isUnsolvable() && !currentLP.isSolved() && !isTimeout()) {
        debugAsymptoticBound("Currently handling:");
        debugAsymptoticBound(currentLP);
        debugAsymptoticBound("");

        InftyExpressionSet::const_iterator it;

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryRemovingConstant(it)) {
                goto start;
            }
        }

        //if the problem is polynomial, try a (max)SMT encoding
        if (smtApplicable && currentLP.isPolynomial()) {
            if (trySmtEncoding()) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryTrimmingPolynomial(it)) {
                goto start;
            }
        }

        if (trySubstitutingVariable()) {
            goto start;
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryReducingExp(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryReducingGeneralExp(it)) {
                goto start;
            }
        }

        if (tryInstantiatingVariable()) {
            goto start;
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (it->hasAtMostOneVariable() && tryApplyingLimitVector(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (it->hasAtLeastTwoVariables() && tryApplyingLimitVectorSmartly(it)) {
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
        debugAsymptoticBound("Limit problem is unsolvable, throwing away");

    } else if (currentLP.isSolved()) {
        solvedLimitProblems.push_back(currentLP);

        proofout << "Solved the limit problem by the following transformations:" << endl;
        proofout.increaseIndention();
        proofout << currentLP.getProof();
        proofout.decreaseIndention();

        if (isAdequateSolution(currentLP)) {
            debugAsymptoticBound("Found adequate solution.");
            debugAsymptoticBound("Proof:" << std::endl << currentLP.getProof());
            return true;
        } else {
            debugAsymptoticBound("Found non-adequate solution.");
            debugAsymptoticBound("Proof:" << std::endl << currentLP.getProof());
        }

    } else {
        debugAsymptoticBound("I don't know how to continue, throwing away");
    }

    if (limitProblems.empty() || isTimeout()) {
        return !solvedLimitProblems.empty();

    } else {
        currentLP = std::move(limitProblems.back());
        limitProblems.pop_back();
        goto start;
    }
}


AsymptoticBound::ComplexityResult AsymptoticBound::getComplexity(const LimitProblem &limitProblem) {
    debugAsymptoticBound("Calculating complexity.");
    ComplexityResult res;

    res.solution = std::move(calcSolution(limitProblem));
    res.upperBound = findUpperBoundforSolution(limitProblem, res.solution);

    for (auto const &pair : res.solution) {
        if (!is_a<numeric>(pair.second)) {
            ++res.inftyVars;
        }
    }

    debugAsymptoticBound(res.inftyVars << " infty var(s)");

    if (res.inftyVars == 0) {
        debugAsymptoticBound("Complexity: None, no infty var!");
        res.complexity = Complexity::Unknown;
    } else if (res.upperBound == 0) {
        debugAsymptoticBound("Complexity: INF (unbounded runtime)");
        res.complexity = Complexity::Infty;
    } else {
        res.lowerBound = findLowerBoundforSolvedCost(limitProblem, res.solution);

        ExprSymbol n = limitProblem.getN();
        if (res.lowerBound < 0) { // lower bound is exponential
            res.lowerBound = -res.lowerBound;
            debugAsymptoticBound("Complexity: Omega(" << res.lowerBound << "^" << "(" << n << "^"
                                 << "(1/" << res.upperBound << ")" << ")" << ")" << std::endl);
            res.complexity = Complexity::Exp;

            // Note: 2^sqrt(n) is not exponential, we just give up such cases (where the exponent might be sublinear)
            // Example: cost 2^y with guard x > y^2
            if (res.upperBound > 1) {
                res.complexity = Complexity::Unknown;
                debugWarn("Complexity is possibly sub-exponential, giving up (solution: " << res.solution << ")");
            }

        } else {
            debugAsymptoticBound("Complexity: Omega(" << n << "^" << "(" << res.lowerBound
                                 << "/" << res.upperBound << ")" << ")" << std::endl);

            res.complexity = Complexity::Poly(res.lowerBound, res.upperBound);
        }
    }

    if (res.complexity > bestComplexity.complexity) {
        bestComplexity = res;
    }

    return res;
}


bool AsymptoticBound::isAdequateSolution(const LimitProblem &limitProblem) {
    debugAsymptoticBound("Checking solution for adequateness.");
    assert(limitProblem.isSolved());

    ComplexityResult result = getComplexity(limitProblem);

    if (result.complexity == Complexity::Infty) {
        return true;
    }

    if (cost.getComplexity()  > result.complexity) {
        return false;
    }

    Expression solvedCost = cost.subs(result.solution).expand();
    ExprSymbol n = limitProblem.getN();
    debugAsymptoticBound("solved cost: " << solvedCost);
    debugAsymptoticBound("cost: " << cost);

    if (solvedCost.is_polynomial(n)) {
        if (!cost.info(info_flags::polynomial)) {
            return false;
        }

        if (cost.getMaxDegree() > solvedCost.degree(n)) {
            return false;
        }

    }

    for (const ExprSymbol &var : cost.getVariables()) {
        if (varMan.isTempVar(ex_to<symbol>(var))) {
            // we try to achieve ComplexInfty
            return false;
        }
    }

    return true;
}


void AsymptoticBound::createBacktrackingPoint(const InftyExpressionSet::const_iterator &it,
                                              Direction dir) {
    assert(dir == POS_INF || dir == POS_CONS);

    if (finalCheck && it->getDirection() == POS) {
        limitProblems.push_back(currentLP);
        limitProblems.back().addExpression(InftyExpression(*it, dir));

        debugAsymptoticBound("creating backtracking point:");
        debugAsymptoticBound(limitProblems.back());
        debugAsymptoticBound("");
    }
}


bool AsymptoticBound::tryRemovingConstant(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.removeConstantIsApplicable(it)) {
        debugAsymptoticBound("removing constant");

        currentLP.removeConstant(it);
        return true;

    } else {
        return false;
    }
}


bool AsymptoticBound::tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.trimPolynomialIsApplicable(it)) {
        debugAsymptoticBound("trimming polynomial");
        createBacktrackingPoint(it, POS_CONS);

        currentLP.trimPolynomial(it);

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingExp(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reduceExpIsApplicable(it)) {
        debugAsymptoticBound("reducing exp");
        createBacktrackingPoint(it, POS_CONS);

        currentLP.reduceExp(it);

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingGeneralExp(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reduceGeneralExpIsApplicable(it)) {
        debugAsymptoticBound("reducing exp (general)");
        createBacktrackingPoint(it, POS_CONS);

        currentLP.reduceGeneralExp(it);

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryApplyingLimitVector(const InftyExpressionSet::const_iterator &it) {
    std::vector<LimitVector> *limitVectors;

    Expression l, r;
    if (it->isProperRational()) {
        debugAsymptoticBound(*it << " is a proper rational");
        l = it->numer();
        r = it->denom();

        limitVectors = &division[it->getDirection()];

    } else if (is_a<add>(*it)) {
        debugAsymptoticBound(*it << " is an addition");
        l = numeric(0);
        r = numeric(0);
        int pos = 0;

        for (int i = 0; i <= pos; ++i) {
            l += it->op(i);
        }
        for (int i = pos + 1; i < it->nops(); ++i) {
            r += it->op(i);
        }

        limitVectors = &addition[it->getDirection()];

    } else if (is_a<mul>(*it)) {
        debugAsymptoticBound(*it << " is a multiplication");
        l = numeric(1);
        r = numeric(1);
        int pos = 0;

        for (int i = 0; i <= pos; ++i) {
            l *= it->op(i);
        }
        for (int i = pos + 1; i < it->nops(); ++i) {
            r *= it->op(i);
        }

        limitVectors = &multiplication[it->getDirection()];

    } else if (it->isProperNaturalPower()) {
        debugLimitProblem(*it << " is a proper natural power");
        Expression base = it->op(0);
        numeric power = ex_to<numeric>(it->op(1));

        if (power.is_even()) {
            l = pow(base, power / 2);
            r = l;
        } else {
            l = base;
            r = pow(base, power - 1);
        }

        limitVectors = &multiplication[it->getDirection()];

    } else {
        return false;
    }

    debugAsymptoticBound("trying to apply limit vectors");
    return applyLimitVectorsThatMakeSense(it, l, r, *limitVectors);
}


bool AsymptoticBound::tryApplyingLimitVectorSmartly(const InftyExpressionSet::const_iterator &it) {
    Expression l, r;
    std::vector<LimitVector> *limitVectors;
    if (is_a<add>(*it)) {
        l = numeric(0);
        r = numeric(0);

        bool foundOneVar = false;
        ExprSymbol oneVar;
        for (int i = 0; i < it->nops(); ++i) {
            Expression ex(it->op(i));

            if (ex.hasNoVariables()) {
                l = ex;
                r = *it - ex;
                break;

            } else if (ex.hasExactlyOneVariable()) {
                if (!foundOneVar) {
                    foundOneVar = true;
                    oneVar = ex.getAVariable();
                    l = ex;

                } else if (oneVar == ex.getAVariable()) {
                    l += ex;

                } else {
                    r += ex;
                }
            } else {
                r += ex;
            }
        }

        if (l.is_zero() || r.is_zero()) {
            return false;
        }

        limitVectors = &addition[it->getDirection()];
    } else if (is_a<mul>(*it)) {
        l = numeric(1);
        r = numeric(1);

        bool foundOneVar = false;
        ExprSymbol oneVar;
        for (int i = 0; i < it->nops(); ++i) {
            Expression ex(it->op(i));

            if (ex.hasNoVariables()) {
                l = ex;
                r = *it / ex;
                break;

            } else if (ex.hasExactlyOneVariable()) {
                if (!foundOneVar) {
                    foundOneVar = true;
                    oneVar = ex.getAVariable();
                    l = ex;

                } else if (oneVar == ex.getAVariable()) {
                    l *= ex;

                } else {
                    r *= ex;
                }
            } else {
                r *= ex;
            }
        }

        if (l == numeric(1) || r == numeric(1)) {
            return false;
        }

        limitVectors = &multiplication[it->getDirection()];
    } else {
        return false;
    }

    debugAsymptoticBound("trying to apply limit vectors (smart)");
    return applyLimitVectorsThatMakeSense(it, l, r, *limitVectors);
}


bool AsymptoticBound::applyLimitVectorsThatMakeSense(const InftyExpressionSet::const_iterator &it,
                                                     const Expression &l, const Expression &r,
                                                     const std::vector<LimitVector> &limitVectors) {
    debugAsymptoticBound("expression: " << *it);
    debugAsymptoticBound("l: " << l);
    debugAsymptoticBound("r: " << r);
    debugAsymptoticBound("applicable limit vectors:");
    bool posInfVector = false;
    bool posConsVector = false;
    toApply.clear();
    for (const LimitVector &lv: limitVectors) {
        if (lv.makesSense(l, r)) {
            debugAsymptoticBound(lv << " makes sense");
            toApply.push_back(lv);

            if (lv.getType() == POS_INF) {
                posInfVector = true;
            } else if (lv.getType() == POS_CONS) {
                posConsVector = true;
            }
        } else {
            debugAsymptoticBound(lv << " does not make sense");
        }
    }
    debugAsymptoticBound("");

    if (posInfVector && !posConsVector) {
        createBacktrackingPoint(it, POS_CONS);
    }
    if (posConsVector && !posInfVector) {
        createBacktrackingPoint(it, POS_INF);
    }

    if (!toApply.empty()) {
        for (int i = 0; i < toApply.size() - 1; ++i) {
            limitProblems.push_back(currentLP);
            LimitProblem &copy = limitProblems.back();
            auto copyIt = copy.find(*it);

            copy.applyLimitVector(copyIt, l, r, toApply[i]);

            if (copy.isUnsolvable()) {
                limitProblems.pop_back();
            }
        }

        currentLP.applyLimitVector(it, l, r, toApply.back());

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryInstantiatingVariable() {
    for (auto it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
        Direction dir = it->getDirection();

        if (it->hasExactlyOneVariable() && (dir == POS || dir == POS_CONS || dir == NEG_CONS)) {
            Z3Context context;
            z3::model model(context, Z3_model());
            z3::check_result result;
            result = Z3Toolbox::checkAll(currentLP.getQuery(), context, &model);

            if (result == z3::unsat) {
                debugAsymptoticBound("Z3: limit problem is unsat");
                currentLP.setUnsolvable();

            } else if (result == z3::sat) {
                ExprSymbol var = it->getAVariable();

                debugAsymptoticBound("Z3: limit problem is sat");
                Expression rational =
                    Z3Toolbox::getRealFromModel(model, GinacToZ3::convert(var, context));

                exmap sub;
                sub.insert(std::make_pair(var, rational));
                substitutions.push_back(std::move(sub));

                debugAsymptoticBound("instantiating " << var << " with " << rational);

                createBacktrackingPoint(it, POS_INF);
                currentLP.substitute(substitutions.back(), substitutions.size() - 1);

            } else {
                debugAsymptoticBound("Z3: limit problem is unknown");

                if (!finalCheck && currentLP.getSize() >= Config::Limit::ProblemDiscardSize) {
                    debugAsymptoticBound("marking the current limit problem as unsolvable"
                                         << " since this is not the final check"
                                         << " and it is very large (" << currentLP.getSize()
                                         << " InftyExpressions)");
                    currentLP.setUnsolvable();
                }

                return false;
            }

            return true;
        }
    }

    return false;
}


bool AsymptoticBound::trySubstitutingVariable() {
    InftyExpressionSet::const_iterator it, it2;
    for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
        if (is_a<symbol>(*it)) {
            for (it2 = std::next(it); it2 != currentLP.cend(); ++it2) {
                if (is_a<symbol>(*it2)) {
                    Direction dir = it->getDirection();
                    Direction dir2 = it2->getDirection();

                    if (((dir == POS || dir == POS_INF) && (dir2 == POS || dir2 == POS_INF))
                        || (dir == NEG_INF && dir2 == NEG_INF)) {
                        assert(*it != *it2);

                        debugAsymptoticBound("substituting variable " << *it << " by " << *it2);

                        exmap sub;
                        sub.insert(std::make_pair(*it, *it2));
                        substitutions.push_back(sub);

                        createBacktrackingPoint(it, POS_CONS);
                        createBacktrackingPoint(it2, POS_CONS);
                        currentLP.substitute(sub, substitutions.size() - 1);

                        return true;
                    }
                }
            }
        }
    }

    return false;
}


bool AsymptoticBound::trySmtEncoding() {
    auto optSubs = LimitSmtEncoding::applyEncoding(currentLP, cost, varMan, finalCheck);
    if (!optSubs) return false;

    substitutions.push_back(optSubs.get());
    currentLP.removeAllConstraints();
    currentLP.substitute(substitutions.back(), substitutions.size() - 1);
    return true;
}


bool AsymptoticBound::isTimeout() const {
    return finalCheck ? Timeout::hard() : Timeout::soft();
}


AsymptoticBound::Result AsymptoticBound::determineComplexity(const VarMan &varMan,
                                                             const GuardList &guard,
                                                             const Expression &cost,
                                                             bool finalCheck)
{
    if (finalCheck) Timing::start(Timing::Asymptotic);
    debugAsymptoticBound("Analyzing asymptotic bound.");

    // Expand the cost to make it easier to analyze
    Expression expandedCost = cost.expand();

#ifdef DEBUG_ASYMPTOTIC_BOUNDS
    debugAsymptoticBound("guard:");
    for (const Expression &ex : guard) {
        debugAsymptoticBound(ex);
    }
    debugAsymptoticBound(endl << "cost:" << expandedCost << endl);
#endif

    // Handle nontermination
    if (expandedCost.isInfSymbol()) {
        return Result(Complexity::Nonterm, Expression::InfSymbol, false, 0);
    }
    assert(!expandedCost.has(Expression::InfSymbol));

    // Only enable proof output for the final check (we don't want proof output when pruning)
    bool wasProofEnabled = proofout.setEnabled(finalCheck && Config::Output::ProofLimit);

    AsymptoticBound asymptoticBound(varMan, guard, expandedCost, finalCheck);
    asymptoticBound.initLimitVectors();
    asymptoticBound.normalizeGuard();

    // Otherwise perform limit calculus
    asymptoticBound.createInitialLimitProblem();
    asymptoticBound.propagateBounds();
    asymptoticBound.removeUnsatProblems();
    bool result = asymptoticBound.solveLimitProblem();
    if (finalCheck) Timing::done(Timing::Asymptotic);

    if (result) {
        debugAsymptoticBound("Solved the initial limit problem. ("
                << asymptoticBound.solvedLimitProblems.size()
                << " solved problem(s))");

        // Print solution
        proofout << "Solution:" << std::endl;
        proofout.increaseIndention();
        for (const auto &pair : asymptoticBound.bestComplexity.solution) {
            proofout << pair.first << " / " << pair.second << std::endl;
        }
        proofout.decreaseIndention();
        proofout.setEnabled(wasProofEnabled);

        // Gather all relevant information
        Expression solvedCost = asymptoticBound.cost.subs(asymptoticBound.bestComplexity.solution);
        return Result(asymptoticBound.bestComplexity.complexity,
                      solvedCost.expand(),
                      asymptoticBound.bestComplexity.upperBound > 1,
                      asymptoticBound.bestComplexity.inftyVars);
    } else {
        debugAsymptoticBound("Could not solve the initial limit problem");

        proofout << "Could not solve the limit problem." << endl;
        proofout.setEnabled(wasProofEnabled);

        return Result(Complexity::Unknown);
    }
}
