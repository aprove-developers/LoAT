#include "limitproblem.h"

#include <z3++.h>

#include "debug.h"
#include "z3toolbox.h"

using namespace GiNaC;

const char* InftyDirectionNames[] = { "+", "-", "+!", "-!", "+/+!"};

// limit vectors for addition
const std::vector<LimitVector> LimitVector::Addition = {
    // increasing limit vectors
    LimitVector(POS_INF, POS_INF, POS_INF),
    LimitVector(POS_INF, POS_INF, POS_CONS),
    LimitVector(POS_INF, POS_CONS, POS_INF),
    LimitVector(POS_INF, POS_INF, NEG_CONS),
    LimitVector(POS_INF, NEG_CONS, POS_INF),

    // decreasing limit vectors
    LimitVector(NEG_INF, NEG_INF, NEG_INF),
    LimitVector(NEG_INF, NEG_INF, NEG_CONS),
    LimitVector(NEG_INF, NEG_CONS, NEG_INF),
    LimitVector(NEG_INF, NEG_INF, POS_CONS),
    LimitVector(NEG_INF, POS_CONS, NEG_INF),

    // positive limit vectors
    LimitVector(POS_CONS, POS_CONS, POS_CONS),

    // negative limit vectors
    LimitVector(NEG_CONS, NEG_CONS, NEG_CONS)
};

// limit vectors for multiplication
const std::vector<LimitVector> LimitVector::Multiplication = {
    // increasing limit vectors
    LimitVector(POS_INF, POS_INF, POS_INF),
    LimitVector(POS_INF, POS_INF, POS_CONS),
    LimitVector(POS_INF, POS_CONS, POS_INF),
    LimitVector(POS_INF, NEG_INF, NEG_INF),
    LimitVector(POS_INF, NEG_INF, NEG_CONS),
    LimitVector(POS_INF, NEG_CONS, NEG_INF),

    // decreasing limit vectors
    LimitVector(NEG_INF, NEG_INF, POS_INF),
    LimitVector(NEG_INF, POS_INF, NEG_INF),
    LimitVector(NEG_INF, NEG_INF, POS_CONS),
    LimitVector(NEG_INF, POS_CONS, NEG_INF),
    LimitVector(NEG_INF, POS_INF, NEG_CONS),
    LimitVector(NEG_INF, NEG_CONS, POS_INF),

    // positive limit vectors
    LimitVector(POS_CONS, POS_CONS, POS_CONS),
    LimitVector(POS_CONS, NEG_CONS, NEG_CONS),

    // negative limit vectors
    LimitVector(NEG_CONS, POS_CONS, NEG_CONS),
    LimitVector(NEG_CONS, NEG_CONS, POS_CONS)
};

// limit vectors for division
const std::vector<LimitVector> LimitVector::Division = {
    // increasing limit vectors
    LimitVector(POS_INF, POS_INF, POS_CONS),
    LimitVector(POS_INF, NEG_INF, NEG_CONS),

    // decreasing limit vectors
    LimitVector(NEG_INF, NEG_INF, POS_CONS),
    LimitVector(NEG_INF, POS_INF, NEG_CONS),

    // positive limit vectors
    LimitVector(POS_CONS, POS_CONS, POS_CONS),
    LimitVector(POS_CONS, NEG_CONS, NEG_CONS),

    // negative limit vectors
    LimitVector(NEG_CONS, NEG_CONS, POS_CONS),
    LimitVector(NEG_CONS, POS_CONS, NEG_CONS)
};

LimitVector::LimitVector(InftyDirection type, InftyDirection first, InftyDirection second)
    : type(type), first(first), second(second) {
    assert(type != POS && first != POS && second != POS);
}


InftyDirection LimitVector::getType() const {
    return type;
}


InftyDirection LimitVector::getFirst() const {
    return first;
}


InftyDirection LimitVector::getSecond() const {
    return second;
}


bool LimitVector::isApplicable(InftyDirection dir) const {
    return  (dir == getType())
            || (dir == POS && (getType() == POS_INF
                               || getType() == POS_CONS));
}


std::ostream& operator<<(std::ostream &os, const LimitVector &lv) {
    os << InftyDirectionNames[lv.getType()] << " limit vector "
       << "(" << InftyDirectionNames[lv.getFirst()] << ","
       << InftyDirectionNames[lv.getSecond()] << ")";

    return os;
}


InftyExpression::InftyExpression(InftyDirection dir) {
    setDirection(dir);
}


InftyExpression::InftyExpression(const GiNaC::basic &other, InftyDirection dir)
    : Expression(other) {
    setDirection(dir);
}


InftyExpression::InftyExpression(const GiNaC::ex &other, InftyDirection dir)
    : Expression(other) {
    setDirection(dir);
}


void InftyExpression::setDirection(InftyDirection dir) {
    direction = dir;
}


InftyDirection InftyExpression::getDirection() const {
    return direction;
}


void InftyExpression::dump(const std::string &description) const {
#ifdef DEBUG_LIMIT_PROBLEMS
    std::cout << description << ": " << *this << " ("
              << InftyDirectionNames[getDirection()] << ")" << std::endl;
#endif
}


LimitProblem::LimitProblem()
    : variableN("n"), unsolvable(false) {
}

LimitProblem::LimitProblem(const GuardList &normalizedGuard, const Expression &cost)
    : variableN("n"), unsolvable(false) {
    for (const Expression &ex : normalizedGuard) {
        assert(GuardToolbox::isNormalizedInequality(ex));

        addExpression(InftyExpression(ex.lhs()));
    }

    assert(!is_a<relational>(cost));
    addExpression(InftyExpression(cost, InftyDirection::POS_INF));

    dump("Created initial limit problem");
}


LimitProblem::LimitProblem(const LimitProblem &other)
    : set(other.set),
      variableN(other.variableN),
      substitutions(other.substitutions),
      unsolvable(other.unsolvable) {
}


LimitProblem& LimitProblem::operator=(const LimitProblem &other) {
    if (this != &other) {
        set = other.set;
        variableN = other.variableN;
        substitutions = other.substitutions;
        unsolvable = other.unsolvable;
    }

    return *this;
}


LimitProblem::LimitProblem(LimitProblem &&other)
    : set(std::move(other.set)),
      variableN(other.variableN),
      substitutions(std::move(other.substitutions)),
      unsolvable(other.unsolvable) {
}


LimitProblem& LimitProblem::operator=(LimitProblem &&other) {
    if (this != &other) {
        set = std::move(other.set);
        variableN = other.variableN;
        substitutions = std::move(other.substitutions);
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
    InftyDirection dir = it->getDirection();

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

    debugLimitProblem("applying transformation rule (A), replacing " << *it
                      << " (" << InftyDirectionNames[dir] << ") by "
                      << firstExp << " (" << InftyDirectionNames[lv.getFirst()] << ") and "
                      << secondExp << " (" << InftyDirectionNames[lv.getSecond()] << ")"
                      << " using " << lv);

    set.erase(it);
    addExpression(InftyExpression(firstExp, lv.getFirst()));
    addExpression(InftyExpression(secondExp, lv.getSecond()));

    dump("resulting limit problem");
}


void LimitProblem::applyLimitVectorAdvanced(const InftyExpressionSet::const_iterator &it, Expression l, Expression r,
                                    const LimitVector &lv) {
    InftyDirection dir = it->getDirection();

    assert(lv.isApplicable(dir));

    debugLimitProblem("applying transformation rule (A) (advanced), replacing " << *it
                      << " (" << InftyDirectionNames[dir] << ") by "
                      << l << " (" << InftyDirectionNames[lv.getFirst()] << ") and "
                      << r << " (" << InftyDirectionNames[lv.getSecond()] << ")"
                      << " using " << lv);

    set.erase(it);
    addExpression(InftyExpression(l, lv.getFirst()));
    addExpression(InftyExpression(r, lv.getSecond()));

    dump("resulting limit problem");
}


void LimitProblem::removeConstant(const InftyExpressionSet::const_iterator &it) {
    InftyDirection dir = it->getDirection();

    assert(it->info(info_flags::integer));
    numeric num = ex_to<numeric>(*it);
    assert((num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS));

    debugLimitProblem("applying transformation rule (B), deleting " << *it
                      << " (" << InftyDirectionNames[dir] << ")");

    set.erase(it);

    dump("resulting limit problem");
}


void LimitProblem::substitute(const GiNaC::exmap &sub, int substitutionIndex) {
    for (auto s : sub) {
        assert(!s.second.has(s.first));
    }

    debugLimitProblem("applying transformation rule (C) using substitution " << sub);

    InftyExpressionSet oldSet = set;
    set.clear();
    for (const InftyExpression &ex : oldSet) {
        addExpression(InftyExpression(ex.subs(sub), ex.getDirection()));
    }

    substitutions.push_back(substitutionIndex);

    dump("resulting limit problem");
}


void LimitProblem::trimPolynomial(const InftyExpressionSet::const_iterator &it) {
    ExprSymbolSet variables = it->getVariables();

    // the expression has to be a univariate polynomial
    assert(it->info(info_flags::polynomial));
    assert(variables.size() == 1);

    InftyDirection dir = it->getDirection();
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

        debugLimitProblem("applying transformation rule (D), replacing " << *it
                      << " (" << InftyDirectionNames[it->getDirection()] << ") by "
                      << leadingTerm << " (" << InftyDirectionNames[dir] << ")");

        set.erase(it);
        addExpression(InftyExpression(leadingTerm, dir));
    } else {
        debugLimitProblem(*it << " is already a monom");
    }

    dump("resulting limit problem");
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

    debugLimitProblem("applying transformation rule (E), replacing " << *it
                      << " (" << InftyDirectionNames[it->getDirection()] << ") by "
                      << (a - 1) << " (" << InftyDirectionNames[POS] << ") and "
                      << e << " (" << InftyDirectionNames[POS_INF] << ")");

    set.erase(it);
    addExpression(InftyExpression(a - 1, POS));
    addExpression(InftyExpression(e, POS_INF));

    dump("resulting limit problem");
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

    debugLimitProblem("applying transformation rule (F), replacing " << *it
                      << " (" << InftyDirectionNames[it->getDirection()] << ") by "
                      << (a - 1) << " (" << InftyDirectionNames[POS] << ") and "
                      << (e + b) << " (" << InftyDirectionNames[POS_INF] << ")");

    set.erase(it);
    addExpression(InftyExpression(a - 1, POS));
    addExpression(InftyExpression(e + b, POS_INF));

    dump("resulting limit problem");
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


bool LimitProblem::isUnsolvable() const {
    return unsolvable;
}

void LimitProblem::setUnsolvable() {
    unsolvable = true;
}


bool LimitProblem::removeConstantIsApplicable(const InftyExpressionSet::const_iterator &it) {
    if (!it->info(info_flags::integer)) {
        return false;
    }

    numeric num = ex_to<numeric>(*it);
    InftyDirection dir = it->getDirection();

    return (num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS);
}


bool LimitProblem::trimPolynomialIsApplicable(const InftyExpressionSet::const_iterator &it) {
    ExprSymbolSet variables = it->getVariables();

    if (!it->info(info_flags::polynomial) || !(variables.size() == 1)) {
        return false;
    }

    InftyDirection dir = it->getDirection();
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


void LimitProblem::dump(const std::string &description) const {
#ifdef DEBUG_LIMIT_PROBLEMS
    std::cout << description << ":" << std::endl;
    for (auto ex : set) {
        std::cout << ex << " (" << InftyDirectionNames[ex.getDirection()] << "), ";
    }
    std::cout << std::endl;

    std::cout << "the problem is " << (isUnsolvable() ? "unsolvable" : (isSolved() ? "solved" : "not solved"))
              << std::endl << std::endl;
#endif
}