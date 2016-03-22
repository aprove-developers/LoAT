#include "asymptoticbound.h"

#include <iterator>
#include <vector>
#include <ginac/ginac.h>
#include <z3++.h>

#include "expression.h"
#include "guardtoolbox.h"
#include "timeout.h"
#include "z3toolbox.h"

using namespace GiNaC;

AsymptoticBound::AsymptoticBound(const ITRSProblem &its, GuardList guard,
                                 Expression cost, bool finalCheck)
    : its(its), guard(guard), cost(cost), finalCheck(finalCheck),
      addition(DirectionSize), multiplication(DirectionSize), division(DirectionSize) {
    assert(GuardToolbox::isValidGuard(guard));
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
        assert(ex.info(info_flags::relation_equal)
               || GuardToolbox::isValidInequality(ex));

        Expression target = ex.rhs() - ex.lhs();
        if (ex.info(info_flags::relation_equal)
            && target.info(info_flags::polynomial)) {

            std::vector<ExprSymbol> vars;
            std::vector<ExprSymbol> tempVars;
            for (const ExprSymbol & var : target.getVariables()) {
                if (its.isFreeVar(var)) {
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
                if (GuardToolbox::solveTermFor(target, var,
                                                GuardToolbox::PropagationLevel::NoCoefficients)) {
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
    debugAsymptoticBound("resulting solution: " << solution << std::endl);

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

        if (!its.isFreeVar(ex_to<symbol>(pair.first))) {
            Expression sub = pair.second;
            assert(sub.is_polynomial(n));
            assert(sub.getVariables().empty()
                   || (sub.getVariables().size() == 1 && sub.getVariables().count(n) > 0));

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

        // code the lowerBound as a negative number to mark it as exponential
        lowerBound = -lowerBound;
    }

    return lowerBound;
}

void AsymptoticBound::removeUnsatProblems() {
    for (int i = limitProblems.size() - 1; i >= 0; --i) {
        if (limitProblems[i].isUnsat()) {
            debugAsymptoticBound("unsat:");
            debugAsymptoticBound(limitProblems[i]);
            limitProblems.erase(limitProblems.begin() + i);
        }
    }
}


bool AsymptoticBound::solveLimitProblem() {
    debugAsymptoticBound("Trying to solve the initial limit problem.");

    if (limitProblems.empty()) {
        return false;
    }

    currentLP = std::move(limitProblems.back());
    limitProblems.pop_back();

    start:
    if (!currentLP.isUnsolvable() && !currentLP.isSolved()) {
        debugAsymptoticBound("Currently handling:");
        debugAsymptoticBound(currentLP);
        debugAsymptoticBound("");

        InftyExpressionSet::const_iterator it;

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryRemovingConstant(it)) {
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
            if (tryReducingPolynomialPower(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryReducingGeneralPower(it)) {
                goto start;
            }
        }

        if (tryInstantiatingVariable()) {
            goto start;
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (it->getVariables().size() <= 1 && tryApplyingLimitVector(it)) {
                goto start;
            }
        }

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (it->getVariables().size() >= 2 && tryApplyingLimitVectorSmartly(it)) {
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
        debugAsymptoticBound(currentLP);

    } else if (currentLP.isSolved()) {
        solvedLimitProblems.push_back(currentLP);

        if (isAdequateSolution(currentLP)) {
            return true;
        } else {
            debugAsymptoticBound("Found non-adequate solution.");
        }

    } else {
        debugAsymptoticBound("I don't know how to continue, throwing away");
        debugAsymptoticBound(currentLP);
    }

    if (limitProblems.empty()) {
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

    if (res.upperBound == 0) {
        res.complexity = Expression::ComplexInfty;
    } else {
        res.lowerBound = findLowerBoundforSolvedCost(limitProblem, res.solution);

        ExprSymbol n = limitProblem.getN();
        if (res.lowerBound < 0) { // lower bound is exponential
            res.lowerBound = -res.lowerBound;
            debugAsymptoticBound("Omega(" << res.lowerBound << "^" << "(" << n << "^"
                                 << "(1/" << res.upperBound << ")" << ")" << ")" << std::endl);

            res.complexity = Expression::ComplexExp;
        } else {
            debugAsymptoticBound("Omega(" << n << "^" << "(" << res.lowerBound
                                 << "/" << res.upperBound << ")" << ")" << std::endl);

            res.complexity = Complexity(res.lowerBound, res.upperBound);
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

    debugAsymptoticBound("PROOF:");
    debugAsymptoticBound(limitProblem.log.str());

    ComplexityResult result = getComplexity(limitProblem);

    if (result.complexity == Expression::ComplexInfty) {
        return true;
    }

    if (cost.getComplexity()  > result.complexity) {
        return false;
    }

    Expression solvedCost = cost.subs(result.solution);
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

    for (const ExprSymbol &var : cost.getVariables()) {
        if (its.isFreeVar(ex_to<symbol>(var))) {
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
        currentLP.removeConstant(it);
        return true;

    } else {
        return false;
    }
}


bool AsymptoticBound::tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.trimPolynomialIsApplicable(it)) {
        createBacktrackingPoint(it, POS_CONS);

        currentLP.trimPolynomial(it);

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingPolynomialPower(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reducePolynomialPowerIsApplicable(it)) {
        createBacktrackingPoint(it, POS_CONS);

        currentLP.reducePolynomialPower(it);

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingGeneralPower(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reduceGeneralPowerIsApplicable(it)) {
        createBacktrackingPoint(it, POS_CONS);

        currentLP.reduceGeneralPower(it);

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
        debugAsymptoticBound(*it << " is neither a proper rational, an addition,"
                             << " a multiplication, nor a proper natural power");
        return false;
    }

    debugAsymptoticBound("trying to apply limit vectors");
    return applyLimitVectorsThatMakeSense(it, l, r, *limitVectors);
}


bool AsymptoticBound::tryApplyingLimitVectorSmartly(const InftyExpressionSet::const_iterator &it) {
    if (!(is_a<add>(*it) || is_a<mul>(*it))) {
        return false;
    }

    std::vector<Expression> ops(it->begin(), it->end());

    ExprSymbolSet minVars = std::move(ops[0].getVariables());
    for (int i = 1; i < it->nops(); ++i) {
        ExprSymbolSet vars = std::move(ops[i].getVariables());
        if (vars.size() < minVars.size()) {
            minVars = std::move(vars);
        }
    }

    debugAsymptoticBound("minVars: ");
    for (const ExprSymbol &var : minVars) {
        debugAsymptoticBound(var);
    }

    Expression l, r;
    std::vector<LimitVector> *limitVectors;
    if (is_a<add>(*it)) {
        l = numeric(0);
        r = numeric(0);
        for (int i = 0; i < it->nops(); ++i) {
            if (ops[i].getVariables() == minVars) {
                l += ops[i];
            } else {
                r += ops[i];
            }
        }

        if (l.is_zero() || r.is_zero()) {
            return false;
        }

        limitVectors = &addition[it->getDirection()];
    } else {
        l = numeric(1);
        r = numeric(1);
        for (int i = 0; i < it->nops(); ++i) {
            if (ops[i].getVariables() == minVars) {
                l *= ops[i];
            } else {
                r *= ops[i];
            }
        }

        if (l == numeric(1) || r == numeric(1)) {
            return false;
        }

        limitVectors = &multiplication[it->getDirection()];
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
    std::vector<LimitVector> toApply;
    bool posInfVector = false;
    bool posConsVector = false;
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
        ExprSymbolSet vars = std::move(it->getVariables());
        Direction dir = it->getDirection();

        if (vars.size() == 1 && (dir == POS || dir == POS_CONS || dir == NEG_CONS)) {
            Z3VariableContext context;
            z3::model model(context, Z3_model());
            z3::check_result result;
            result = Z3Toolbox::checkExpressionsSAT(currentLP.getQuery(), context, &model);

            if (result == z3::unsat) {
                debugAsymptoticBound("Z3: limit problem is unsat");
                debugAsymptoticBound(currentLP);
                currentLP.setUnsolvable();

            } else if (result == z3::sat) {
                ExprSymbol var = *vars.begin();

                debugAsymptoticBound("Z3: limit problem is sat");
                debugAsymptoticBound(currentLP);
                Expression rational =
                    Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(var, context));

                exmap sub;
                sub.insert(std::make_pair(var, rational));
                substitutions.push_back(std::move(sub));

                debugAsymptoticBound("instantiating " << var << " by " << rational);

                createBacktrackingPoint(it, POS_INF);
                currentLP.substitute(substitutions.back(), substitutions.size() - 1);

            } else {
                debugAsymptoticBound("Z3: limit problem is unknown");
                debugAsymptoticBound(currentLP);
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


InfiniteInstances::Result AsymptoticBound::determineComplexity(const ITRSProblem &its,
                                                               const GuardList &guard,
                                                               const Expression &cost,
                                                               bool finalCheck) {
    debugAsymptoticBound("Analyzing asymptotic bound.");

    debugAsymptoticBound("guard:" << std::endl);
    for (const Expression &ex : guard) {
        debugAsymptoticBound(ex);
    }
    debugAsymptoticBound("");
    debugAsymptoticBound("cost:" << std::endl << cost);
    debugAsymptoticBound("");

    Expression expandedCost = cost.expand();
    //if cost contains infty, check if coefficient > 0 is SAT, otherwise remove infty symbol
    if (expandedCost.has(Expression::Infty)) {
        Expression inftyCoeff = expandedCost.coeff(Expression::Infty);

        GuardList query = guard;
        query.push_back(inftyCoeff > 0);

        if (Z3Toolbox::checkExpressionsSAT(query) == z3::sat) {
            return InfiniteInstances::Result(Expression::ComplexInfty, false,
                                             Expression::Infty, 0, "INF coeff sat");
        }

        return InfiniteInstances::Result(Expression::ComplexNone,
                                         "The cost contains infinity.");
    }

    AsymptoticBound asymptoticBound(its, guard, cost, finalCheck);
    asymptoticBound.initLimitVectors();
    asymptoticBound.normalizeGuard();
    asymptoticBound.createInitialLimitProblem();
    asymptoticBound.propagateBounds();
    asymptoticBound.removeUnsatProblems();

    if (asymptoticBound.solveLimitProblem()) {
        debugAsymptoticBound("Solved the initial limit problem.");

        debugAsymptoticBound(asymptoticBound.solvedLimitProblems.size() << " solved problems");

        Expression solvedCost = asymptoticBound.cost.subs(asymptoticBound.bestComplexity.solution);
        return InfiniteInstances::Result(asymptoticBound.bestComplexity.complexity,
                                         asymptoticBound.bestComplexity.upperBound > 1,
                                         solvedCost,
                                         0, "Solved the initial limit problem.");
    } else {
        debugAsymptoticBound("Could not solve the initial limit problem.");

        return InfiniteInstances::Result(Expression::ComplexNone,
                                         "Could not solve the initial limit problem.");
    }
}
