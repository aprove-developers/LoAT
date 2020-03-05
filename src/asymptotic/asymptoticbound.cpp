#include "asymptoticbound.hpp"

#include <iterator>
#include <vector>

#include "../expr/expression.hpp"
#include "../expr/guardtoolbox.hpp"

#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"

#include "limitsmt.hpp"
#include "inftyexpression.hpp"
#include "../util/proofoutput.hpp"

using namespace std;


AsymptoticBound::AsymptoticBound(VarMan &varMan, GuardList guard,
                                 Expression cost, bool finalCheck, uint timeout)
    : varMan(varMan), guard(guard), cost(cost), finalCheck(finalCheck), timeout(timeout),
      addition(DirectionSize), multiplication(DirectionSize), division(DirectionSize), currentLP(varMan) {
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

    for (const Rel &rel : guard) {

        if (rel.getOp() == Rel::eq) {
            // Split equation
            Rel greaterEqual = (rel.lhs() >= rel.rhs()).normalizeInequality();
            Rel lessEqual = (rel.lhs() <= rel.rhs()).normalizeInequality();

            normalizedGuard.push_back(greaterEqual);
            normalizedGuard.push_back(lessEqual);

        } else {
            normalizedGuard.push_back(rel.normalizeInequality());
        }
    }

}

void AsymptoticBound::createInitialLimitProblem(VariableManager &varMan) {
    currentLP = LimitProblem(normalizedGuard, cost, varMan);
}

void AsymptoticBound::propagateBounds() {
    assert(substitutions.size() == 0);

    if (currentLP.isUnsolvable()) {
        return;
    }

    // build substitutions from equations
    for (const Rel &rel : guard) {
        Expression target = rel.rhs() - rel.lhs();
        if (rel.getOp() == Rel::eq
            && rel.isPolynomial()) {

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
                    substitutions.push_back(ExprMap(var, optSolved.get()));
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
    for (const Rel &rel : guard) {
        if (rel.getOp() != Rel::eq) {
            if (rel.lhs().isSymbol() || rel.rhs().isSymbol()) {
                Rel relT = rel.toLessOrLessEq();

                Expression l, r;
                bool swap = relT.rhs().isSymbol();
                if (swap) {
                    l = relT.rhs();
                    r = relT.lhs();
                } else {
                    l = relT.lhs();
                    r = relT.rhs();
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

                if (r.isPolynomial() && !r.has(l)) {
                    if (relT.getOp() == Rel::lt && !swap) { // exT: x = l < r
                        r = r - 1;
                    } else if (relT.getOp() == Rel::lt && swap) { // exT: r < l = x
                        r = r + 1;
                    }
                    substitutions.push_back(ExprMap(l, r));
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

ExprMap AsymptoticBound::calcSolution(const LimitProblem &limitProblem) {
    assert(limitProblem.isSolved());

    ExprMap solution;
    for (int index : limitProblem.getSubstitutions()) {
        const ExprMap &sub = substitutions[index];

        solution = sub.compose(solution);
    }

    solution = limitProblem.getSolution().compose(solution);

    GuardList guardCopy = guard;
    guardCopy.push_back(cost > 0);
    for (const Rel &rel : guardCopy) {
        for (const ExprSymbol &var : rel.getVariables()) {
            if (!solution.contains(var)) {
                solution = ExprMap(var, 0).compose(solution);
            }
        }
    }

    return solution;
}


int AsymptoticBound::findUpperBoundforSolution(const LimitProblem &limitProblem,
                                               const ExprMap &solution) {
    ExprSymbol n = limitProblem.getN();
    int upperBound = 0;
    for (auto const &pair : solution) {
        assert(pair.first.isSymbol());

        if (!varMan.isTempVar(pair.first.toSymbol())) {
            Expression sub = pair.second;
            assert(sub.isPolynomial(n));
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
                                                 const ExprMap &solution) {

    Expression solvedCost = cost.subs(solution);

    int lowerBound;
    ExprSymbol n = limitProblem.getN();
    if (solvedCost.isPolynomial()) {
        assert(solvedCost.isPolynomial(n));
        assert(solvedCost.hasAtMostOneVariable());

        Expression expanded = solvedCost.expand();
        int d = expanded.degree(n);
        lowerBound = d;

    } else {
        std::vector<Expression> nonPolynomial;
        Expression expanded = solvedCost.expand();

        Expression powerPattern = Expression::wildcard(1) ^ Expression::wildcard(2);
        ExpressionSet powers;
        assert(expanded.findAll(powerPattern, powers));

        lowerBound = 1;
        for (const Expression &ex : powers) {

            if (ex.op(1).has(n) && ex.op(1).isPolynomial(n)) {
                assert(ex.op(0).isIntegerConstant());
                assert(ex.op(0).toNumeric().is_positive());

                int base =  ex.op(0).toNumeric().to_int();
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
        auto result = Smt::check(buildAnd(limitProblems[i].getQuery()), varMan);

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
    if (!currentLP.isUnsolvable() && !currentLP.isSolved()) {

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

    if (limitProblems.empty()) {
        return !solvedLimitProblems.empty();

    } else {
        currentLP = std::move(limitProblems.back());
        limitProblems.pop_back();
        goto start;
    }
}


AsymptoticBound::ComplexityResult AsymptoticBound::getComplexity(const LimitProblem &limitProblem) {
    ComplexityResult res;

    res.solution = calcSolution(limitProblem);
    res.upperBound = findUpperBoundforSolution(limitProblem, res.solution);

    for (auto const &pair : res.solution) {
        if (!pair.second.isNumeric()) {
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

    if (solvedCost.isPolynomial(n)) {
        if (!cost.isPolynomial()) {
            return false;
        }

        if (cost.getMaxDegree() > solvedCost.degree(n)) {
            return false;
        }

    }

    for (const ExprSymbol &var : cost.getVariables()) {
        if (varMan.isTempVar(var)) {
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

    } else if (it->isAdd()) {
        l = 0;
        r = 0;
        int pos = 0;

        for (int i = 0; i <= pos; ++i) {
            l = l + it->op(i);
        }
        for (unsigned int i = pos + 1; i < it->nops(); ++i) {
            r = r + it->op(i);
        }

        limitVectors = &addition[it->getDirection()];

    } else if (it->isMul()) {
        l = 1;
        r = 1;
        int pos = 0;

        for (int i = 0; i <= pos; ++i) {
            l = l * it->op(i);
        }
        for (unsigned int i = pos + 1; i < it->nops(); ++i) {
            r = r * it->op(i);
        }

        limitVectors = &multiplication[it->getDirection()];

    } else if (it->isProperNaturalPower()) {
        Expression base = it->op(0);
        GiNaC::numeric power = it->op(1).toNumeric();

        if (power.is_even()) {
            l = base ^ (power / 2);
            r = l;
        } else {
            l = base;
            r = base ^ (power - 1);
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
    if (it->isAdd()) {
        l = 0;
        r = 0;

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
                    l = l + ex;

                } else {
                    r = r + ex;
                }
            } else {
                r = r + ex;
            }
        }

        if (l.is_zero() || r.is_zero()) {
            return false;
        }

        limitVectors = &addition[it->getDirection()];
    } else if (it->isMul()) {
        l = 1;
        r = 1;

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
                    l = l * ex;

                } else {
                    r = r * ex;
                }
            } else {
                r = r * ex;
            }
        }

        if (l.is_equal(1) || r.is_equal(1)) {
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
            const std::vector<Rel> &query = currentLP.getQuery();
            std::unique_ptr<Smt> solver = SmtFactory::modelBuildingSolver(Smt::chooseLogic<UpdateMap>({query}, {}), varMan);
            solver->add(buildAnd(query));
            Smt::Result result = solver->check();

            if (result == Smt::Unsat) {
                currentLP.setUnsolvable();

            } else if (result == Smt::Sat) {
                const ExprSymbolMap<GiNaC::numeric> &model = solver->model();
                ExprSymbol var = it->getAVariable();

                Expression rational = model.at(var);
                substitutions.push_back(ExprMap(var, rational));

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
        if (it->isSymbol()) {
            for (it2 = std::next(it); it2 != currentLP.cend(); ++it2) {
                if (it2->isSymbol()) {
                    Direction dir = it->getDirection();
                    Direction dir2 = it2->getDirection();

                    if (((dir == POS || dir == POS_INF) && (dir2 == POS || dir2 == POS_INF))
                        || (dir == NEG_INF && dir2 == NEG_INF)) {
                        assert(!it->is_equal(*it2));

                        ExprMap sub(*it, *it2);
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
    auto optSubs = LimitSmtEncoding::applyEncoding(currentLP, cost, varMan, currentRes, timeout);
    if (!optSubs) return false;

    substitutions.push_back(optSubs.get());
    currentLP.removeAllConstraints();
    currentLP.substitute(substitutions.back(), substitutions.size() - 1);
    return true;
}


AsymptoticBound::Result AsymptoticBound::determineComplexity(VarMan &varMan,
                                                             const GuardList &guard,
                                                             const Expression &cost,
                                                             bool finalCheck,
                                                             const Complexity &currentRes,
                                                             uint timeout) {

    // Expand the cost to make it easier to analyze
    Expression expandedCost = cost.expand();
    Expression costToCheck = expandedCost;

    // Handle nontermination. It suffices to check that the guard is satisfiable
    if (expandedCost.isNontermSymbol()) {
        auto smtRes = Smt::check(buildAnd(guard), varMan);
        if (smtRes == Smt::Sat) {
            ProofOutput proof;
            proof.append("Guard is satisfiable, yielding nontermination");
            return Result(Complexity::Nonterm, Expression::NontermSymbol, 0, proof);
        } else {
            // if Z3 fails, the calculus for limit problems might still succeed if, e.g., the rule contains exponentials
            costToCheck = varMan.getVarSymbol(varMan.addFreshVariable("x"));
        }
    }
    if (finalCheck && Config::Analysis::NonTermMode) {
        return Result(Complexity::Unknown);
    }
    assert(!costToCheck.has(Expression::NontermSymbol));

    AsymptoticBound asymptoticBound(varMan, guard, costToCheck, finalCheck, timeout);
    asymptoticBound.initLimitVectors();
    asymptoticBound.normalizeGuard();

    asymptoticBound.createInitialLimitProblem(varMan);
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
            return Result(Complexity::Nonterm, Expression::NontermSymbol, 0, asymptoticBound.proof);
        } else {
            Expression solvedCost = asymptoticBound.cost.subs(asymptoticBound.bestComplexity.solution);
            return Result(asymptoticBound.bestComplexity.complexity,
                          solvedCost.expand(),
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
                                                                    Complexity currentRes,
                                                                    uint timeout) {
    Expression expandedCost = cost.expand();
    // Handle nontermination. It suffices to check that the guard is satisfiable
    if (expandedCost.isNontermSymbol()) {
        auto smtRes = Smt::check(buildAnd(guard), varMan);
        if (smtRes == Smt::Sat) {
            ProofOutput proof;
            proof.append("proved non-termination via SMT");
            return Result(Complexity::Nonterm, Expression::NontermSymbol, 0, proof);
        } else {
            return Result(Complexity::Unknown);
        }
    } else if (finalCheck && Config::Analysis::NonTermMode) {
        return Result(Complexity::Unknown);
    }
    assert(!expandedCost.has(Expression::NontermSymbol));

    AsymptoticBound asymptoticBound(varMan, guard, expandedCost, false, timeout);
    asymptoticBound.initLimitVectors();
    asymptoticBound.normalizeGuard();
    asymptoticBound.createInitialLimitProblem(varMan);
    bool success = asymptoticBound.solveViaSMT(currentRes);
    if (success) {
        Expression solvedCost = asymptoticBound.cost.subs(asymptoticBound.bestComplexity.solution);
        return Result(asymptoticBound.bestComplexity.complexity,
                      solvedCost.expand(),
                      asymptoticBound.bestComplexity.inftyVars,
                      asymptoticBound.proof);
    } else {
        return Result(Complexity::Unknown);
    }
}
