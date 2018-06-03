#include "asymptoticbound.h"

#include <iterator>
#include <vector>
#include <ginac/ginac.h>
#include <z3++.h>

#include "expr/expression.h"
#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "util/timeout.h"

#include "z3/z3solver.h"
#include "z3/z3context.h"
#include "z3/z3toolbox.h"

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
                if (GuardToolbox::solveTermFor(target, var, GuardToolbox::TrivialCoeffs)) {
                    exmap sub;
                    sub.insert(std::make_pair(var, target));

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
                   && limitProblems[i].getSize() >= LIMIT_PROBLEM_DISCARD_SIZE) {
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

    bool smtApplicable = GlobalFlags::limitSmt && isSmtApplicable();

    currentLP = std::move(limitProblems.back());
    limitProblems.pop_back();

    start:
    if (!currentLP.isUnsolvable() && !currentLP.isSolved() && !Timeout::hard()) {
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
        if (smtApplicable && currentLP.isPolynomial(varMan.getGinacVarList())) {
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

    if (limitProblems.empty() || Timeout::hard()) {
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

    Expression solvedCost = cost.subs(result.solution);
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

                if (!finalCheck && currentLP.getSize() >= LIMIT_PROBLEM_DISCARD_SIZE) {
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



bool AsymptoticBound::isSmtApplicable() {
    return cost.is_polynomial(varMan.getGinacVarList());
}

/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies that
 * lim_{n->\infty} p is a positive constant
 */
z3::expr posConstraint(const map<int, Expression>& coefficients, Z3Context& context) {
    z3::expr conjunction = context.bool_val(true);
    for (pair<int, Expression> p : coefficients) {
        int degree = p.first;
        Expression c = p.second;
        if (degree > 0) {
            conjunction = conjunction && c.toZ3(context) == 0;
        } else {
            conjunction = conjunction && c.toZ3(context) > 0;
        }
    }
    return conjunction;
}

/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies that
 * lim_{n->\infty} p is a negative constant
 */
z3::expr negConstraint(const map<int, Expression>& coefficients, Z3Context& context) {
    z3::expr conjunction = context.bool_val(true);
    for (pair<int, Expression> p : coefficients) {
        int degree = p.first;
        Expression c = p.second;
        if (degree > 0) {
            conjunction = conjunction && c.toZ3(context) == 0;
        } else {
            conjunction = conjunction && c.toZ3(context) < 0;
        }
    }
    return conjunction;
}

/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies
 * lim_{n->\infty} p = -\infty
 */
z3::expr negInfConstraint(const map<int, Expression>& coefficients, Z3Context& context) {
    int maxDegree = 0;
    for (pair<int, Expression> p: coefficients) {
        maxDegree = p.first > maxDegree ? p.first : maxDegree;
    }
    z3::expr disjunction = context.bool_val(false);
    for (int i = 1; i <= maxDegree; i++) {
        z3::expr conjunction = context.bool_val(true);
        for (pair<int, Expression> p: coefficients) {
            int degree = p.first;
            Expression c = p.second;
            if (degree > i) {
                conjunction = conjunction && c.toZ3(context) == 0;
            } else if (degree == i) {
                conjunction = conjunction && c.toZ3(context) < 0;
            }
        }
        disjunction = disjunction || conjunction;
    }
    return disjunction;
}

/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies
 * lim_{n->\infty} p = \infty
 */
z3::expr posInfConstraint(const map<int, Expression>& coefficients, Z3Context& context) {
    int maxDegree = 0;
    for (pair<int, Expression> p: coefficients) {
        maxDegree = p.first > maxDegree ? p.first : maxDegree;
    }
    z3::expr disjunction = context.bool_val(false);
    for (int i = 1; i <= maxDegree; i++) {
        z3::expr conjunction = context.bool_val(true);
        for (pair<int, Expression> p: coefficients) {
            int degree = p.first;
            Expression c = p.second;
            if (degree > i) {
                conjunction = conjunction && c.toZ3(context) == 0;
            } else if (degree == i) {
                conjunction = conjunction && c.toZ3(context) > 0;
            }
        }
        disjunction = disjunction || conjunction;
    }
    return disjunction;
}

/**
 * @return the (abstract) coefficients of 'n' in 'ex', where the key is the degree of the respective monomial
 */
map<int, Expression> getCoefficients(Expression ex, ExprSymbol n) {
    GiNaC::lst nList;
    nList.append(n);
    int maxDegree = ex.getMaxDegree(nList);
    map<int, Expression> coefficients;
    for (int i = 0; i <= maxDegree; i++) {
        coefficients.emplace(i, ex.coeff(n, i));
    }
    return coefficients;
}

bool AsymptoticBound::trySmtEncoding() {
    debugAsymptoticBound(endl << "SMT: " << currentLP << endl);

    Z3Context context;
    InftyExpressionSet::const_iterator it;

    // initialize z3
    Z3Solver solver(context, Z3_LIMITSMT_TIMEOUT);

    // the parameter of the desired family of solutions
    ExprSymbol n = currentLP.getN();

    // get all relevant variables
    ExprSymbolSet vars = currentLP.getVariables();

    // create linear templates for all variables
    exmap templateSubs;
    map<ExprSymbol,z3::expr,GiNaC::ex_is_less> varCoeff, varCoeff0;
    for (const ExprSymbol &var : vars) {
        ExprSymbol c0 = varMan.getFreshUntrackedSymbol(var.get_name()+"_0");
        ExprSymbol c = varMan.getFreshUntrackedSymbol(var.get_name()+"_c");
        varCoeff.emplace(var,GinacToZ3::convert(c,context)); //HACKy HACK  // FIXME: HACKy HACK sounds dangerous...
        varCoeff0.emplace(var,GinacToZ3::convert(c0,context));
        templateSubs[var] = c0 + (n * c);
    }

    // replace variables in the cost function with their linear templates
    Expression templateCost = cost.subs(templateSubs).expand();

    // if the cost function is a constant, then we are bound to fail
    GiNaC::lst nList;
    nList.append(n);
    int maxDeg = templateCost.getMaxDegree(nList);
    if (maxDeg == 0) {
        return false;
    }

    // encode every entry of the limit problem
    for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
        // replace variables with their linear templates
        Expression ex = (*it).subs(templateSubs).expand();
        map<int, Expression> coefficients = getCoefficients(ex, n);
        Direction direction = it->getDirection();
        // add the required constraints (depending on the direction-label from the limit problem)
        if (direction == POS) {
            z3::expr disjunction = posConstraint(coefficients, context) || posInfConstraint(coefficients, context);
            solver.add(disjunction);
        } else if (direction == POS_CONS) {
            solver.add(posConstraint(coefficients, context));
        } else if (direction == POS_INF) {
            solver.add(posInfConstraint(coefficients, context));
        } else if (direction == NEG_CONS) {
            solver.add(negConstraint(coefficients, context));
        } else if (direction == NEG_INF) {
            solver.add(negInfConstraint(coefficients, context));
        }
    }

    // auxiliary function that checks satisfiability wrt. the current state of the solver
    auto checkSolver = [&]() -> bool {
        debugAsymptoticBound("SMT Query: " << solver);
        z3::check_result res = solver.check();
        debugAsymptoticBound("SMT Result: " << res);
        if (res == z3::sat) {
            return true;
        } else {
            return false;
        }
    };

    // all constraints that we have so far are mandatory, so fail if we can't even solve these
    if (!checkSolver()) {
        return false;
    }

    // remember the current state for backtracking before trying several variations
    solver.push();

    // first fix that all program variables have to be constants
    // a model witnesses unbounded complexity
    for (const ExprSymbol &var : vars) {
        if (!varMan.isTempVar(var)) {
            solver.add(varCoeff.at(var) == 0);
        }
    }

    if (!checkSolver()) {
        // we failed to find a model -- drop all non-mandatory constraints
        solver.pop();
        // try to find a witness for polynomial complexity with degree maxDeg,...,1
        map<int, Expression> coefficients = getCoefficients(templateCost, n);
        for (int i = maxDeg; i > 0; i--) {
            Expression c = coefficients.find(i)->second;
            // remember the current state for backtracking
            solver.push();
            solver.add(c.toZ3(context) > 0);
            if (checkSolver()) {
                break;
            } else if (i == 1) {
                // we even failed to prove a linear bound -- give up
                return false;
            } else {
                // remove all non-mandatory constraints and retry with degree i-1
                solver.pop();
            }
        }
    }

    debugAsymptoticBound("SMT Model: " << solver.get_model() << endl);

    // we found a model -- create the corresponding solution of the limit problem
    exmap smtSubs;
    z3::model model = solver.get_model();
    for (const ExprSymbol &var : vars) {
        Expression c0 = Z3Toolbox::getRealFromModel(model,varCoeff0.at(var));
        Expression c = Z3Toolbox::getRealFromModel(model,varCoeff.at(var));
        smtSubs[var] = c0 + c * n;
    }

    substitutions.push_back(std::move(smtSubs));
    currentLP.removeAllConstraints();
    currentLP.substitute(substitutions.back(), substitutions.size() - 1);
    return true;
}



InfiniteInstances::Result AsymptoticBound::determineComplexity(const VarMan &varMan,
                                                               const GuardList &guard,
                                                               const Expression &cost,
                                                               bool finalCheck) {
    debugAsymptoticBound("Analyzing asymptotic bound.");

    debugAsymptoticBound("guard:");
    for (const Expression &ex : guard) {
        debugAsymptoticBound(ex);
    }
    debugAsymptoticBound("");
    debugAsymptoticBound("cost:" << cost.expand());
    debugAsymptoticBound("");

    Expression expandedCost = cost.expand();

    // TODO: Simplify this check by only using isInfSymbol (and otherwise asserting that InfSymbol does not occur),
    // TODO: See Expression::isInfSymbol for the explanation.
    //if cost contains infty, check if coefficient > 0 is SAT, otherwise remove infty symbol
    if (expandedCost.has(Expression::InfSymbol)) {
        Expression inftyCoeff = expandedCost.coeff(Expression::InfSymbol);

        GuardList query = guard;
        query.push_back(inftyCoeff > 0);

        if (Z3Toolbox::checkAll(query) == z3::sat) {
            return InfiniteInstances::Result(Complexity::Nonterm, false,
                                             Expression::InfSymbol, 0, "NONTERM (INF coeff > 0 by SMT)");
        }

        return InfiniteInstances::Result(Complexity::Unknown, "The cost contains infinity");
    }

    AsymptoticBound asymptoticBound(varMan, guard, cost, finalCheck);
    asymptoticBound.initLimitVectors();
    asymptoticBound.normalizeGuard();

    //otherwise perform limit calculus
    asymptoticBound.createInitialLimitProblem();
    asymptoticBound.propagateBounds();
    asymptoticBound.removeUnsatProblems();
    if (asymptoticBound.solveLimitProblem()) {
        debugAsymptoticBound("Solved the initial limit problem. ("
                << asymptoticBound.solvedLimitProblems.size()
                << " solved problem(s))");

        proofout << "Solution:" << std::endl;
        proofout.increaseIndention();
        for (const auto &pair : asymptoticBound.bestComplexity.solution) {
            proofout << pair.first << " / " << pair.second << std::endl;
        }
        proofout.decreaseIndention();

        Expression solvedCost = asymptoticBound.cost.subs(asymptoticBound.bestComplexity.solution);
        return InfiniteInstances::Result(asymptoticBound.bestComplexity.complexity,
                asymptoticBound.bestComplexity.upperBound > 1,
                solvedCost.expand(),
                asymptoticBound.bestComplexity.inftyVars,
                "Solved the initial limit problem");
    } else {
        debugAsymptoticBound("Could not solve the initial limit problem");

        return InfiniteInstances::Result(Complexity::Unknown,
                false,
                numeric(0),
                cost.getVariables().size(),
                "Could not solve the initial limit problem");
    }
}
