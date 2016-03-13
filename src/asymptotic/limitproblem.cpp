#include "limitproblem.h"

#include "debug.h"

using namespace GiNaC;

const char* InftyDirectionNames[] = { "+", "-", "+!", "-!", "+/+!"};


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


LimitProblem::LimitProblem()
    : variableN("n") {
}

LimitProblem::LimitProblem(const GuardList &normalizedGuard, const Expression &cost)
    : variableN("n") {
    for (const Expression &ex : normalizedGuard) {
        assert(GuardToolbox::isNormalizedInequality(ex));

        addExpression(InftyExpression(ex.lhs()));
    }

    assert(!is_a<relational>(cost));
    addExpression(InftyExpression(cost, InftyDirection::POS_INF));

    dump("Created initial limit problem");
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
                throw LimitProblemIsContradictoryException();
            }
        }
    }
}


InftyExpressionSet::iterator LimitProblem::cbegin() const {
    return set.cbegin();
}


InftyExpressionSet::iterator LimitProblem::cend() const {
    return set.cend();
}


void LimitProblem::applyLimitVector(const InftyExpressionSet::const_iterator &it, int pos,
                                    InftyDirection lvType, InftyDirection first, InftyDirection second) {
    InftyDirection dir = it->getDirection();

    if (it->nops() > 0) {
        assert(pos >= 0 && pos < it->nops());
    }
    assert(dir == lvType || (dir == POS && (lvType == POS_INF || lvType == POS_CONS)));

    for (int i = 0; i < it->nops(); i++) {
        debugLimitProblem("op(" << i << "): " << it->op(i));
    }

    Expression firstExp, secondExp;
    if (it->info(info_flags::rational)) {
        debugLimitProblem(*it << " is a rational");
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

    } else if (is_a<power>(*it)) {
        debugLimitProblem(*it << " is a power");
        Expression base = it->op(0);
        Expression power = it->op(1);
        assert(power.info(info_flags::integer) && (power - 1).info(info_flags::positive));

        firstExp = pow(base, pos + 1);
        secondExp = pow(base, power - pos - 1);

    } else {
        debugLimitProblem(*it << " is neither a rational, an addition, a multiplication nor a power");
        assert(false);
    }

    debugLimitProblem("applying transformation rule (A), replacing " << *it
                      << " (" << InftyDirectionNames[dir] << ") by "
                      << firstExp << " (" << InftyDirectionNames[first] << ") and "
                      << secondExp << " (" << InftyDirectionNames[second] << ")");

    set.erase(it);
    addExpression(InftyExpression(firstExp, first));
    addExpression(InftyExpression(secondExp, second));

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


void LimitProblem::substitute(const GiNaC::exmap &sub) {
    for (auto s : sub) {
        assert(!s.second.has(s.first));
    }

    debugLimitProblem("applying transformation rule (C) using substitution " << sub);

    InftyExpressionSet oldSet = set;
    set.clear();
    for (const InftyExpression &ex : oldSet) {
        addExpression(InftyExpression(ex.subs(sub), ex.getDirection()));
    }

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


bool LimitProblem::isSolved() const {
    // Check if an expression is not a variable
    for (const InftyExpression &ex : set) {
        if (!is_a<symbol>(ex)) {
            debugLimitProblem(ex << " is not a variable");

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

void LimitProblem::dump(const std::string &description) const {
#ifdef DEBUG_LIMIT_PROBLEMS
    std::cout << description << ":" << std::endl;
    for (auto ex : set) {
        std::cout << ex << " (" << InftyDirectionNames[ex.getDirection()] << "), ";
    }
    std::cout << std::endl;

    std::cout << "the problem is " << (isSolved() ? "solved" : "not solved")
              << std::endl << std::endl;
#endif
}

void LimitProblem::solve(LimitProblem &problem) {
}