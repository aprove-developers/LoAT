#include "limitproblem.h"

#include "debug.h"

using namespace GiNaC;

const char* InftyDirectionNames[] = { "+", "-", "+!", "-!", "+/+!"};


InftyExpression::InftyExpression(InftyDirection dir)
    : Expression() {
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


LimitProblem::LimitProblem(const GuardList &normalizedGuard, const Expression &cost) {
    for (const Expression &ex : normalizedGuard) {
        assert(GuardToolbox::isNormalizedInequality(ex));

        add(InftyExpression(ex.lhs()));
    }

    assert(!is_a<relational>(cost));
    add(InftyExpression(cost, InftyDirection::POS_INF));

    dump("Created initial limit problem");
}


void LimitProblem::add(const InftyExpression &ex) {
    InftyExpressionSet::iterator it = set.find(ex);

    if (it == set.end()) {
        set.insert(ex);
    } else {
        if (it->getDirection() != ex.getDirection()) {
            if (it->getDirection() == POS &&
                (ex.getDirection() == POS_INF || ex.getDirection() == POS_CONS)) {
                set.erase(it);
                set.insert(ex);

            } else {
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

/*
void LimitProblem::applyLimitVector(int index, int pos, InftyDir lvType,
                                    InftyDir first, InftyDir second) {
    assert(index >= 0 && index < expressions.size());

    Expression ex = expressions[index].first;
    InftyDir dir = expressions[index].second;

    if (ex.nops() > 0) {
        assert(pos >= 0 && pos < ex.nops());
    }
    assert(dir == lvType || (dir == POS && (lvType == POS_INF || lvType == POS_CONS)));

    for (int i = 0; i < ex.nops(); i++) {
        debugLimitProblem("op(" << i << "): " << ex.op(i));
    }

    Expression firstExp, secondExp;
    if (ex.info(info_flags::rational)) {
        debugLimitProblem(ex << " is a rational");
        firstExp = ex.numer();
        secondExp = ex.denom();

    } else if (is_a<add>(ex)) {
        debugLimitProblem(ex << " is an addition");
        firstExp = numeric(0);
        secondExp = numeric(0);

        for (int i = 0; i <= pos; ++i) {
            firstExp += ex.op(i);
        }
        for (int i = pos + 1; i < ex.nops(); ++i) {
            secondExp += ex.op(i);
        }

    } else if (is_a<mul>(ex)) {
        debugLimitProblem(ex << " is a multiplication");
        firstExp = numeric(1);
        secondExp = numeric(1);

        for (int i = 0; i <= pos; ++i) {
            firstExp *= ex.op(i);
        }
        for (int i = pos + 1; i < ex.nops(); ++i) {
            secondExp *= ex.op(i);
        }

    } else if (is_a<power>(ex)) {
        debugLimitProblem(ex << " is a power");
        Expression base = ex.op(0);
        Expression power = ex.op(1);
        assert(power.info(info_flags::integer) && power.info(info_flags::positive));

        firstExp = pow(base, pos + 1);
        secondExp = pow(base, power - pos - 1);

    } else {
        debugLimitProblem(ex << " is neither a rational, an addition, a multiplication nor a power");
        assert(false);
    }

    debugLimitProblem("applying transformation rule (A), replacing " << ex
                      << " (" << InftyDirNames[dir] << ") by "
                      << firstExp << " (" << InftyDirNames[first] << ") and "
                      << secondExp << " (" << InftyDirNames[second] << ")");

    expressions.erase(expressions.begin() + index);
    expressions.push_back(InftyExpression(firstExp, first));
    expressions.push_back(InftyExpression(secondExp, second));

    dump("resulting limit problem");
}*/


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
        set.insert(InftyExpression(leadingTerm, dir));
    } else {
        debugLimitProblem(*it << "is already a monom");
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