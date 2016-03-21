#include "limitproblem.h"

#include <z3++.h>
#include <sstream>
#include <utility>

#include "debug.h"
#include "z3toolbox.h"

using namespace GiNaC;

LimitProblem::LimitProblem()
    : variableN("n"), unsolvable(false) {
}

LimitProblem::LimitProblem(const GuardList &normalizedGuard, const Expression &cost)
    : variableN("n"), unsolvable(false) {
    for (const Expression &ex : normalizedGuard) {
        assert(GuardToolbox::isNormalizedInequality(ex));

        addExpression(InftyExpression(ex.lhs(), POS));
    }

    assert(!is_a<relational>(cost));
    addExpression(InftyExpression(cost, POS_INF));

    debugLimitProblem("Created initial limit problem:");
    debugLimitProblem(*this);
}


LimitProblem::LimitProblem(const LimitProblem &other)
    : set(other.set),
      variableN(other.variableN),
      substitutions(other.substitutions),
      log(other.log),
      unsolvable(other.unsolvable) {
}


LimitProblem& LimitProblem::operator=(const LimitProblem &other) {
    if (this != &other) {
        set = other.set;
        variableN = other.variableN;
        substitutions = other.substitutions;
        log = other.log;
        unsolvable = other.unsolvable;
    }

    return *this;
}


LimitProblem::LimitProblem(LimitProblem &&other)
    : set(std::move(other.set)),
      variableN(other.variableN),
      substitutions(std::move(other.substitutions)),
      log(std::move(other.log)),
      unsolvable(other.unsolvable) {
}


LimitProblem& LimitProblem::operator=(LimitProblem &&other) {
    if (this != &other) {
        set = std::move(other.set);
        variableN = other.variableN;
        substitutions = std::move(other.substitutions);
        log = std::move(other.log);
        unsolvable = other.unsolvable;
    }

    return *this;
}


void LimitProblem::addExpression(const InftyExpression &ex) {
    InftyExpressionSet::iterator it = set.find(ex);

    if (it == set.end()) {
        set.insert(ex);
    } else {
        if (it->getDirection() != ex.getDirection()) {
            if (it->getDirection() == POS &&
                (ex.getDirection() == POS_INF || ex.getDirection() == POS_CONS)) {
                set.erase(it);
                set.insert(ex);

            } else if (!(ex.getDirection() == POS &&
                         (it->getDirection() == POS_INF || it->getDirection() == POS_CONS))) {
                unsolvable = true;
            }
        }
    }

    if ((ex.getDirection() == POS_INF || ex.getDirection() == NEG_INF)
        && is_a<numeric>(ex)) {
        unsolvable = true;
    }

    if (((ex.getDirection() == POS_CONS || ex.getDirection() == POS) && is_a<numeric>(ex) && (ex.info(info_flags::negative) || ex.is_zero()))
        || (ex.getDirection() == NEG_CONS && is_a<numeric>(ex) && ex.info(info_flags::nonnegative))) {
        unsolvable = true;
    }
}


InftyExpressionSet::iterator LimitProblem::cbegin() const {
    return set.cbegin();
}


InftyExpressionSet::iterator LimitProblem::cend() const {
    return set.cend();
}


void LimitProblem::applyLimitVector(const InftyExpressionSet::const_iterator &it, int pos,
                                    const LimitVector &lv) {
    Direction dir = it->getDirection();

    if (it->nops() > 0) {
        assert(pos >= 0 && pos < it->nops());
    }
    assert(lv.isApplicable(dir));

    for (int i = 0; i < it->nops(); i++) {
        debugLimitProblem("op(" << i << "): " << it->op(i));
    }

    Expression firstExp, secondExp;
    if (it->isProperRational()) {
        debugLimitProblem(*it << " is a proper rational");
        firstExp = it->numer();
        secondExp = it->denom();

    } else if (is_a<add>(*it)) {
        debugLimitProblem(*it << " is an addition");
        firstExp = numeric(0);
        secondExp = numeric(0);

        for (int i = 0; i <= pos; ++i) {
            firstExp += it->op(i);
        }
        for (int i = pos + 1; i < it->nops(); ++i) {
            secondExp += it->op(i);
        }

    } else if (is_a<mul>(*it)) {
        debugLimitProblem(*it << " is a multiplication");
        firstExp = numeric(1);
        secondExp = numeric(1);

        for (int i = 0; i <= pos; ++i) {
            firstExp *= it->op(i);
        }
        for (int i = pos + 1; i < it->nops(); ++i) {
            secondExp *= it->op(i);
        }

    } else if (it->isProperNaturalPower()) {
        debugLimitProblem(*it << " is a proper natural power");
        Expression base = it->op(0);
        Expression power = it->op(1);

        firstExp = pow(base, pos + 1);
        secondExp = pow(base, power - pos - 1);

    } else {
        debugLimitProblem(*it << " is neither a proper rational, an addition,"
                          << " a multiplication, nor a proper natural power");
        assert(false);
    }

    std::ostringstream oss;

    oss << "applying transformation rule (A), replacing " << *it
                      << " (" << DirectionNames[dir] << ") by "
                      << firstExp << " (" << DirectionNames[lv.getFirst()] << ") and "
                      << secondExp << " (" << DirectionNames[lv.getSecond()] << ")"
                      << " using " << lv;

    log.push_back(oss.str());
    debugLimitProblem(oss.str());

    set.erase(it);
    addExpression(InftyExpression(firstExp, lv.getFirst()));
    addExpression(InftyExpression(secondExp, lv.getSecond()));

    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
}


void LimitProblem::applyLimitVector(const InftyExpressionSet::const_iterator &it, Expression l, Expression r,
                                    const LimitVector &lv) {
    Direction dir = it->getDirection();

    assert(lv.isApplicable(dir));

    std::ostringstream oss;

    oss << "applying transformation rule (A) (advanced), replacing " << *it
                      << " (" << DirectionNames[dir] << ") by "
                      << l << " (" << DirectionNames[lv.getFirst()] << ") and "
                      << r << " (" << DirectionNames[lv.getSecond()] << ")"
                      << " using " << lv;

    log.push_back(oss.str());
    debugLimitProblem(oss.str());

    set.erase(it);
    addExpression(InftyExpression(l, lv.getFirst()));
    addExpression(InftyExpression(r, lv.getSecond()));

    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
}


void LimitProblem::removeConstant(const InftyExpressionSet::const_iterator &it) {
    Direction dir = it->getDirection();

    assert(it->info(info_flags::integer));
    numeric num = ex_to<numeric>(*it);
    assert((num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS));

    std::ostringstream oss;
    oss << "applying transformation rule (B), deleting " << *it
                      << " (" << DirectionNames[dir] << ")";

    log.push_back(oss.str());
    debugLimitProblem(oss.str());

    set.erase(it);

    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
}


void LimitProblem::substitute(const GiNaC::exmap &sub, int substitutionIndex) {
    for (auto s : sub) {
        assert(!s.second.has(s.first));
    }

    std::ostringstream oss;
    oss << "applying transformation rule (C) using substitution " << sub;

    log.push_back(oss.str());
    debugLimitProblem(oss.str());

    InftyExpressionSet oldSet;
    oldSet.swap(set);
    for (const InftyExpression &ex : oldSet) {
        addExpression(InftyExpression(ex.subs(sub), ex.getDirection()));
    }

    substitutions.push_back(substitutionIndex);

    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
}


void LimitProblem::trimPolynomial(const InftyExpressionSet::const_iterator &it) {
    ExprSymbolSet variables = it->getVariables();

    // the expression has to be a univariate polynomial
    assert(it->info(info_flags::polynomial));
    assert(variables.size() == 1);

    Direction dir = it->getDirection();
    assert((dir == POS) || (dir == POS_INF) || (dir == NEG_INF));

    ExprSymbol var = *variables.begin();

    Expression expanded = it->expand();
    debugLimitProblem("expanded " << *it << " to " << expanded);


    if (is_a<add>(expanded)) {
        Expression leadingTerm = expanded.lcoeff(var) * pow(var, expanded.degree(var));

        debugLimitProblem("the leading term is " << leadingTerm);

        if (dir == POS) {
            // Fix the direction of the expression
            dir = POS_INF;
        }

        std::ostringstream oss;

        oss << "applying transformation rule (D), replacing " << *it
                      << " (" << DirectionNames[it->getDirection()] << ") by "
                      << leadingTerm << " (" << DirectionNames[dir] << ")";

        log.push_back(oss.str());
        debugLimitProblem(oss.str());

        set.erase(it);
        addExpression(InftyExpression(leadingTerm, dir));
    } else {
        debugLimitProblem(*it << " is already a monom");
    }

    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
}


void LimitProblem::reducePolynomialPower(const InftyExpressionSet::const_iterator &it) {
    assert(it->getDirection() == POS_INF || it->getDirection() == POS);

    debugLimitProblem("expression: " << *it);

    ExprSymbolSet variables = it->getVariables();
    assert(variables.size() == 1);
    ExprSymbol x = *variables.cbegin();

    Expression powerInExp;
    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && summand.op(1).has(x)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }
    debugLimitProblem("polynomial power: " << powerInExp);
    assert(is_a<power>(powerInExp));

    Expression b = *it - powerInExp;
    debugLimitProblem("b: " << b);
    assert(b.is_polynomial(x));

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    debugLimitProblem("a: " << a << ", e: " << e);

    assert(a.is_polynomial(x));
    assert(e.is_polynomial(x));
    assert(e.has(x));

    std::ostringstream oss;

    oss << "applying transformation rule (E), replacing " << *it
                      << " (" << DirectionNames[it->getDirection()] << ") by "
                      << (a - 1) << " (" << DirectionNames[POS] << ") and "
                      << e << " (" << DirectionNames[POS_INF] << ")";

    log.push_back(oss.str());
    debugLimitProblem(oss.str());

    set.erase(it);
    addExpression(InftyExpression(a - 1, POS));
    addExpression(InftyExpression(e, POS_INF));

    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
}


void LimitProblem::reduceGeneralPower(const InftyExpressionSet::const_iterator &it) {
    assert(it->getDirection() == POS_INF || it->getDirection() == POS);

    debugLimitProblem("expression: " << *it);

    Expression powerInExp;
    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && (!summand.op(1).info(info_flags::polynomial)
                                         || summand.getVariables().size() > 1)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }
    debugLimitProblem("general power: " << powerInExp);
    assert(is_a<power>(powerInExp));
    assert(!powerInExp.op(1).info(info_flags::polynomial)
           || powerInExp.getVariables().size() > 1);

    Expression b = *it - powerInExp;
    debugLimitProblem("b: " << b);

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    debugLimitProblem("a: " << a << ", e: " << e);

    std::ostringstream oss;

    oss << "applying transformation rule (F), replacing " << *it
                      << " (" << DirectionNames[it->getDirection()] << ") by "
                      << (a - 1) << " (" << DirectionNames[POS] << ") and "
                      << (e + b) << " (" << DirectionNames[POS_INF] << ")";

    log.push_back(oss.str());
    debugLimitProblem(oss.str());

    set.erase(it);
    addExpression(InftyExpression(a - 1, POS));
    addExpression(InftyExpression(e + b, POS_INF));

    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
}


bool LimitProblem::isUnsolvable() const {
    return unsolvable;
}


void LimitProblem::setUnsolvable() {
    unsolvable = true;
}

bool LimitProblem::isSolved() const {
    if (isUnsolvable()) {
        return false;
    }

    // Check if an expression is not a variable
    for (const InftyExpression &ex : set) {
        if (!is_a<symbol>(ex)) {
            //debugLimitProblem(ex << " is not a variable");

            return false;
        }
    }

    // Since the infinity expressions are compared using GiNaC::ex_is_less,
    // the directions do not affect the comparison.
    // Therefore, there cannot be a single variable with different directions.
    return true;
}


GiNaC::exmap LimitProblem::getSolution() const {
    assert(isSolved());

    exmap solution;
    for (const InftyExpression &ex : set) {

        switch (ex.getDirection()) {
            case POS:
            case POS_INF:
                solution.insert(std::make_pair(ex, variableN));
                break;
            case NEG_INF:
                solution.insert(std::make_pair(ex, -variableN));
                break;
            case POS_CONS:
                solution.insert(std::make_pair(ex, numeric(1)));
                break;
            case NEG_CONS:
                solution.insert(std::make_pair(ex, numeric(-1)));
                break;
        }
    }

    return solution;
}


ExprSymbol LimitProblem::getN() const {
    return variableN;
}


const std::vector<int>& LimitProblem::getSubstitutions() const {
    return substitutions;
}


InftyExpressionSet::const_iterator LimitProblem::find(const InftyExpression &ex) {
    return set.find(ex);
}


std::vector<Expression> LimitProblem::getQuery() {
    std::vector<Expression> query;

    InftyExpressionSet::const_iterator it;
    for (it = cbegin(); it != cend(); ++it) {
        if (it->getDirection() == NEG_INF || it->getDirection() == NEG_CONS) {
            query.push_back(it->expand() < 0);
        } else {
            query.push_back(it->expand() > 0);
        }
    }

    return query;
}


bool LimitProblem::isUnsat() {
    return Z3Toolbox::checkExpressionsSAT(getQuery()) == z3::unsat;
}


bool LimitProblem::removeConstantIsApplicable(const InftyExpressionSet::const_iterator &it) {
    if (!it->info(info_flags::integer)) {
        return false;
    }

    numeric num = ex_to<numeric>(*it);
    Direction dir = it->getDirection();

    return (num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS);
}


bool LimitProblem::trimPolynomialIsApplicable(const InftyExpressionSet::const_iterator &it) {
    ExprSymbolSet variables = it->getVariables();

    if (!it->info(info_flags::polynomial) || !(variables.size() == 1)) {
        return false;
    }

    Direction dir = it->getDirection();
    if (!((dir == POS) || (dir == POS_INF) || (dir == NEG_INF))) {
        return false;
    }

    // Check if it is a monom
    return is_a<add>(it->expand());
}


bool LimitProblem::reducePolynomialPowerIsApplicable(const InftyExpressionSet::const_iterator &it) {
    if (!(it->getDirection() == POS_INF || it->getDirection() == POS)) {
        return false;
    }

    ExprSymbolSet variables = it->getVariables();

    if (variables.size() != 1) {
        return false;
    }

    ExprSymbol x = *variables.cbegin();

    Expression powerInExp;
    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && summand.op(1).has(x)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }

    if (!is_a<power>(powerInExp)) {
        return false;
    }

    Expression b = *it - powerInExp;

    if (!b.is_polynomial(x)) {
        return false;
    }

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    if (!(a.is_polynomial(x) && e.is_polynomial(x))) {
        return false;
    }

    return e.has(x);
}


bool LimitProblem::reduceGeneralPowerIsApplicable(const InftyExpressionSet::const_iterator &it) {
    if (!(it->getDirection() == POS_INF || it->getDirection() == POS)) {
        return false;
    }

    Expression powerInExp;
    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && (!summand.op(1).info(info_flags::polynomial)
                                         || summand.getVariables().size() > 1)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }

    return is_a<power>(powerInExp) && (!powerInExp.op(1).info(info_flags::polynomial)
                                       || powerInExp.getVariables().size() > 1);
}


std::ostream& operator<<(std::ostream &os, const LimitProblem &lp) {
    if (lp.cbegin() != lp.cend()) {
        std::copy(lp.cbegin(), --lp.cend(), std::ostream_iterator<InftyExpression>(os, ", "));
        os << *(--lp.cend()) << " ";
    }

    os << "[" << (lp.isUnsolvable() ? "unsolvable"
                  : (lp.isSolved() ? "solved" : "not solved")) << "]";

    return os;
}