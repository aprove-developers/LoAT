#include "asymptoticbound.hpp"

#include <iterator>
#include <vector>
#include <ginac/ginac.h>

#include "../expr/expression.hpp"
#include "../expr/guardtoolbox.hpp"
#include "../expr/relation.hpp"
#include "../util/timeout.hpp"

#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"

#include "limitsmt.hpp"
#include "inftyexpression.hpp"
#include "../util/proofoutput.hpp"

using namespace GiNaC;
using namespace std;


AsymptoticBound::AsymptoticBound(VarMan &varMan, GuardList guard,
                                 Expression cost, bool finalCheck)
    : varMan(varMan), guard(guard), cost(cost), finalCheck(finalCheck),
      addition(DirectionSize), multiplication(DirectionSize), division(DirectionSize) {
    assert(GuardToolbox::isWellformedGuard(guard));
}


void AsymptoticBound::initLimitVectors() {

    for (int i = 0; i < DirectionSize; ++i) {
        Direction dir = static_cast<Direction>(i);

        for (const LimitVector &lv : LimitVector::Addition) {
            if (lv.isApplicable(dir)) {
                addition[dir].push_back(lv);
            }
        }

        for (const LimitVector &lv : LimitVector::Multiplication) {
            if (lv.isApplicable(dir)) {
                multiplication[dir].push_back(lv);
            }
        }

        for (const LimitVector &lv : LimitVector::Division) {
            if (lv.isApplicable(dir)) {
                division[dir].push_back(lv);
            }
        }
    }

}


void AsymptoticBound::normalizeGuard() {

    for (const Expression &ex : guard) {
        assert(Relation::isRelation(ex));

        if (ex.info(info_flags::relation_equal)) {
            // Split equation
            Expression greaterEqual = Relation::normalizeInequality(ex.lhs() >= ex.rhs());
            Expression lessEqual = Relation::normalizeInequality(ex.lhs() <= ex.rhs());

            normalizedGuard.push_back(greaterEqual);
            normalizedGuard.push_back(lessEqual);

        } else {
            Expression normalized = Relation::normalizeInequality(ex);

            normalizedGuard.push_back(normalized);
        }
    }

}

void AsymptoticBound::createInitialLimitProblem() {
    currentLP = LimitProblem(normalizedGuard, cost);
}

void AsymptoticBound::propagateBounds() {
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

                    substitutions.push_back(std::move(sub));
                    break;
                }
            }
        }
    }

    // apply all substitutions resulting from equations
    for (unsigned int i = 0; i < substitutions.size(); ++i) {
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

    for (unsigned int i = numOfEquations; i < substitutions.size(); ++i) {
        limitProblem.substitute(substitutions[i], i);
    }

    if (limitProblem.isUnsolvable()) {
        limitProblems.pop_back();
    }
}

GiNaC::exmap AsymptoticBound::calcSolution(const LimitProblem &limitProblem) {
    assert(limitProblem.isSolved());

    exmap solution;
    for (int index : limitProblem.getSubstitutions()) {
        const exmap &sub = substitutions[index];

        solution = std::move(GuardToolbox::composeSubs(sub, solution));
    }

    solution = std::move(GuardToolbox::composeSubs(limitProblem.getSolution(), solution));

    GuardList guardCopy = guard;
    guardCopy.push_back(cost);
    for (const Expression &ex : guardCopy) {
        for (const ExprSymbol &var : ex.getVariables()) {
            if (solution.count(var) == 0) {
                exmap sub;
                sub.insert(std::make_pair(var, numeric(0)));

                solution = std::move(GuardToolbox::composeSubs(sub, solution));
            }
        }
    }

    return solution;
}


int AsymptoticBound::findUpperBoundforSolution(const LimitProblem &limitProblem,
                                               const GiNaC::exmap &solution) {
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

            if (d > upperBound) {
                upperBound = d;
            }
        }
    }

    return upperBound;
}


int AsymptoticBound::findLowerBoundforSolvedCost(const LimitProblem &limitProblem,
                                                 const GiNaC::exmap &solution) {

    Expression solvedCost = cost.subs(solution);

    int lowerBound;
    ExprSymbol n = limitProblem.getN();
    if (solvedCost.info(info_flags::polynomial)) {
        assert(solvedCost.is_polynomial(n));
        assert(solvedCost.hasAtMostOneVariable());

        Expression expanded = solvedCost.expand();
        int d = expanded.degree(n);
        lowerBound = d;

    } else {
        std::vector<Expression> nonPolynomial;
        Expression expanded = solvedCost.expand();

        Expression powerPattern = pow(wild(1), wild(2));
        std::set<ex, ex_is_less> powers;
        assert(expanded.findAll(powerPattern, powers));

        lowerBound = 1;
        for (const Expression &ex : powers) {

            if (ex.op(1).has(n) && ex.op(1).is_polynomial(n)) {
                assert(ex.op(0).info(info_flags::integer));
                assert(ex.op(0).info(info_flags::positive));

                int base =  ex_to<numeric>(ex.op(0)).to_int();
                if (base > lowerBound) {
                    lowerBound = base;
                }
            }
        }
        assert(lowerBound > 1);

        // code the lowerBound as a negative number to mark it as exponential
        lowerBound = -lowerBound;
    }

    return lowerBound;
}

void AsymptoticBound::removeUnsatProblems() {
    for (int i = limitProblems.size() - 1; i >= 0; --i) {
        auto result = Smt::check(buildAnd(limitProblems[i].getQuery()));

        if (result == Smt::Unsat) {
            limitProblems.erase(limitProblems.begin() + i);
        } else if (result == Smt::Unknown
                   && !finalCheck
                   && limitProblems[i].getSize() >= Config::Limit::ProblemDiscardSize) {
            limitProblems.erase(limitProblems.begin() + i);
        }
    }
}


bool AsymptoticBound::solveViaSMT(Complexity currentRes) {
    if (!Config::Limit::PolyStrategy->smtEnabled() || !currentLP.isPolynomial() || !trySmtEncoding(currentRes)) {
        return false;
    }

    solvedLimitProblems.push_back(currentLP);

    proof.append("Solved the limit problem by the following transformations:");
    proof.append(currentLP.getProof());

    isAdequateSolution(currentLP);
    return true;
}


bool AsymptoticBound::solveLimitProblem() {
    if (limitProblems.empty()) {
        return false;
    }

    currentLP = std::move(limitProblems.back());
    limitProblems.pop_back();

    start:
    if (!currentLP.isUnsolvable() && !currentLP.isSolved() && !isTimeout()) {

        InftyExpressionSet::const_iterator it;

        for (it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
            if (tryRemovingConstant(it)) {
                goto start;
            }
        }

        //if the problem is polynomial, try a (max)SMT encoding
        if (Config::Limit::PolyStrategy->smtEnabled() && currentLP.isPolynomial()) {
            if (trySmtEncoding(Complexity::Const)) {
                goto start;
            } else if (!Config::Limit::PolyStrategy->calculusEnabled()) {
                goto end;
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
    end:

    if (!currentLP.isUnsolvable() && currentLP.isSolved()) {
        solvedLimitProblems.push_back(currentLP);

        proof.append("Solved the limit problem by the following transformations:");
        proof.append(currentLP.getProof());

        if (isAdequateSolution(currentLP)) {
            return true;
        }
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
    ComplexityResult res;

    res.solution = std::move(calcSolution(limitProblem));
    res.upperBound = findUpperBoundforSolution(limitProblem, res.solution);

    for (auto const &pair : res.solution) {
        if (!is_a<numeric>(pair.second)) {
            ++res.inftyVars;
        }
    }

    if (res.inftyVars == 0) {
        res.complexity = Complexity::Unknown;
    } else if (res.upperBound == 0) {
        res.complexity = Complexity::Unbounded;
    } else {
        res.lowerBound = findLowerBoundforSolvedCost(limitProblem, res.solution);

        ExprSymbol n = limitProblem.getN();
        if (res.lowerBound < 0) { // lower bound is exponential
            res.lowerBound = -res.lowerBound;
            res.complexity = Complexity::Exp;

            // Note: 2^sqrt(n) is not exponential, we just give up such cases (where the exponent might be sublinear)
            // Example: cost 2^y with guard x > y^2
            if (res.upperBound > 1) {
                res.complexity = Complexity::Unknown;
            }

        } else {
            res.complexity = Complexity::Poly(res.lowerBound, res.upperBound);
        }
    }

    if (res.complexity > bestComplexity.complexity) {
        bestComplexity = res;
    }

    return res;
}


bool AsymptoticBound::isAdequateSolution(const LimitProblem &limitProblem) {
    assert(limitProblem.isSolved());

    ComplexityResult result = getComplexity(limitProblem);

    if (result.complexity == Complexity::Unbounded) {
        return true;
    }

    if (cost.getComplexity()  > result.complexity) {
        return false;
    }

    Expression solvedCost = cost.subs(result.solution).expand();
    ExprSymbol n = limitProblem.getN();

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


bool AsymptoticBound::tryReducingExp(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reduceExpIsApplicable(it)) {
        createBacktrackingPoint(it, POS_CONS);

        currentLP.reduceExp(it);

        return true;
    } else {
        return false;
    }
}


bool AsymptoticBound::tryReducingGeneralExp(const InftyExpressionSet::const_iterator &it) {
    if (currentLP.reduceGeneralExpIsApplicable(it)) {
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
        l = it->numer();
        r = it->denom();

        limitVectors = &division[it->getDirection()];

    } else if (is_a<add>(*it)) {
        l = numeric(0);
        r = numeric(0);
        int pos = 0;

        for (int i = 0; i <= pos; ++i) {
            l += it->op(i);
        }
        for (unsigned int i = pos + 1; i < it->nops(); ++i) {
            r += it->op(i);
        }

        limitVectors = &addition[it->getDirection()];

    } else if (is_a<mul>(*it)) {
        l = numeric(1);
        r = numeric(1);
        int pos = 0;

        for (int i = 0; i <= pos; ++i) {
            l *= it->op(i);
        }
        for (unsigned int i = pos + 1; i < it->nops(); ++i) {
            r *= it->op(i);
        }

        limitVectors = &multiplication[it->getDirection()];

    } else if (it->isProperNaturalPower()) {
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
        for (unsigned int i = 0; i < it->nops(); ++i) {
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
        for (unsigned int i = 0; i < it->nops(); ++i) {
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

    return applyLimitVectorsThatMakeSense(it, l, r, *limitVectors);
}


bool AsymptoticBound::applyLimitVectorsThatMakeSense(const InftyExpressionSet::const_iterator &it,
                                                     const Expression &l, const Expression &r,
                                                     const std::vector<LimitVector> &limitVectors) {
    bool posInfVector = false;
    bool posConsVector = false;
    toApply.clear();
    for (const LimitVector &lv: limitVectors) {
        if (lv.makesSense(l, r)) {
            toApply.push_back(lv);

            if (lv.getType() == POS_INF) {
                posInfVector = true;
            } else if (lv.getType() == POS_CONS) {
                posConsVector = true;
            }
        }
    }

    if (posInfVector && !posConsVector) {
        createBacktrackingPoint(it, POS_CONS);
    }
    if (posConsVector && !posInfVector) {
        createBacktrackingPoint(it, POS_INF);
    }

    if (!toApply.empty()) {
        for (unsigned int i = 0; i < toApply.size() - 1; ++i) {
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
            std::unique_ptr<Smt> solver = SmtFactory::solver();
            solver->add(buildAnd(currentLP.getQuery()));
            Smt::Result result = solver->check();

            if (result == Smt::Unsat) {
                currentLP.setUnsolvable();

            } else if (result == Smt::Sat) {
                const ExprSymbolMap<GiNaC::numeric> &model = solver->model();
                ExprSymbol var = it->getAVariable();

                Expression rational = model.at(var);

                exmap sub;
                sub.insert(std::make_pair(var, rational));
                substitutions.push_back(std::move(sub));

                createBacktrackingPoint(it, POS_INF);
                currentLP.substitute(substitutions.back(), substitutions.size() - 1);

            } else {

                if (!finalCheck && currentLP.getSize() >= Config::Limit::ProblemDiscardSize) {
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


bool AsymptoticBound::trySmtEncoding(Complexity currentRes) {
    auto optSubs = LimitSmtEncoding::applyEncoding(currentLP, cost, varMan, finalCheck, currentRes);
    if (!optSubs) return false;

    substitutions.push_back(optSubs.get());
    currentLP.removeAllConstraints();
    currentLP.substitute(substitutions.back(), substitutions.size() - 1);
    return true;
}


bool AsymptoticBound::isTimeout() const {
    return finalCheck ? Timeout::hard() : Timeout::soft();
}


AsymptoticBound::Result AsymptoticBound::determineComplexity(VarMan &varMan,
                                                             const GuardList &guard,
                                                             const Expression &cost,
                                                             bool finalCheck,
                                                             const Complexity &currentRes) {

    // Expand the cost to make it easier to analyze
    Expression expandedCost = cost.expand();
    Expression costToCheck = expandedCost;

    // Handle nontermination. It suffices to check that the guard is satisfiable
    if (expandedCost.isNontermSymbol()) {
        auto smtRes = Smt::check(buildAnd(guard));
        if (smtRes == Smt::Sat) {
            ProofOutput proof;
            proof.append("Guard is satisfiable, yielding nontermination");
            return Result(Complexity::Nonterm, Expression::NontermSymbol, false, 0, proof);
        } else {
            // if Z3 fails, the calculus for limit problems might still succeed if, e.g., the rule contains exponentials
            costToCheck = varMan.getVarSymbol(varMan.addFreshVariable("x"));
        }
    }
    if (finalCheck && Config::Analysis::NonTermMode) {
        return Result(Complexity::Unknown);
    }
    assert(!costToCheck.has(Expression::NontermSymbol));

    AsymptoticBound asymptoticBound(varMan, guard, costToCheck, finalCheck);
    asymptoticBound.initLimitVectors();
    asymptoticBound.normalizeGuard();

    asymptoticBound.createInitialLimitProblem();
    // first try the SMT encoding
    bool polynomial = cost.isPolynomial() && asymptoticBound.currentLP.isPolynomial();
    bool result = polynomial && asymptoticBound.solveViaSMT(currentRes);
    if (!result && (!polynomial || Config::Limit::PolyStrategy->calculusEnabled())) {
        // Otherwise perform limit calculus
        asymptoticBound.propagateBounds();
        asymptoticBound.removeUnsatProblems();
        result = asymptoticBound.solveLimitProblem();
    }

    if (result) {

        // Print solution
        asymptoticBound.proof.append("Solution:");
        for (const auto &pair : asymptoticBound.bestComplexity.solution) {
            asymptoticBound.proof.append(stringstream() << pair.first << " / " << pair.second);
        }

        // Gather all relevant information
        if (expandedCost.isNontermSymbol()) {
            return Result(Complexity::Nonterm, Expression::NontermSymbol, false, 0, asymptoticBound.proof);
        } else {
            Expression solvedCost = asymptoticBound.cost.subs(asymptoticBound.bestComplexity.solution);
            return Result(asymptoticBound.bestComplexity.complexity,
                          solvedCost.expand(),
                          asymptoticBound.bestComplexity.upperBound > 1,
                          asymptoticBound.bestComplexity.inftyVars,
                          asymptoticBound.proof);
        }
    } else {

        asymptoticBound.proof.append("Could not solve the limit problem.");

        return Result(Complexity::Unknown);
    }

}

AsymptoticBound::Result AsymptoticBound:: determineComplexityViaSMT(VarMan &varMan,
                                                                    const GuardList &guard,
                                                                    const Expression &cost,
                                                                    bool finalCheck,
                                                                    Complexity currentRes) {
    Expression expandedCost = cost.expand();
    // Handle nontermination. It suffices to check that the guard is satisfiable
    if (expandedCost.isNontermSymbol()) {
        auto smtRes = Smt::check(buildAnd(guard));
        if (smtRes == Smt::Sat) {
            ProofOutput proof;
            proof.append("proved non-termination via SMT");
            return Result(Complexity::Nonterm, Expression::NontermSymbol, false, 0, proof);
        } else {
            return Result(Complexity::Unknown);
        }
    } else if (finalCheck && Config::Analysis::NonTermMode) {
        return Result(Complexity::Unknown);
    }
    assert(!expandedCost.has(Expression::NontermSymbol));

    AsymptoticBound asymptoticBound(varMan, guard, expandedCost, false);
    asymptoticBound.initLimitVectors();
    asymptoticBound.normalizeGuard();
    asymptoticBound.createInitialLimitProblem();
    bool success = asymptoticBound.solveViaSMT(currentRes);
    if (success) {
        Expression solvedCost = asymptoticBound.cost.subs(asymptoticBound.bestComplexity.solution);
        return Result(asymptoticBound.bestComplexity.complexity,
                      solvedCost.expand(),
                      asymptoticBound.bestComplexity.upperBound > 1,
                      asymptoticBound.bestComplexity.inftyVars,
                      asymptoticBound.proof);
    } else {
        return Result(Complexity::Unknown);
    }
}
