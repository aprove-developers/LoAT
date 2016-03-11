#include "limitproblem.h"

#include "debug.h"

using namespace GiNaC;

const char* LimitProblem::InftyDirNames[] = { "+", "-", "+!", "-!", "+/+!"};

LimitProblem::LimitProblem(const GuardList &normalizedGuard, const Expression &cost) {
    for (const Expression &ex : normalizedGuard) {
        assert(GuardToolbox::isNormalizedInequality(ex));

        expressions.push_back(InftyExpression(ex.lhs(), POS));
    }

    assert(!is_a<relational>(cost));
    expressions.push_back(InftyExpression(cost, POS_INF));

    dump("Created initial limit problem");
}


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

        for (int i = 0; i < ex.nops(); ++i) {
            debugLimitProblem(i << ": " << ex.op(i));
        }

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
}


void LimitProblem::removeConstant(int index) {
    assert(index >= 0 && index < expressions.size());

    Expression ex = expressions[index].first;
    InftyDir dir = expressions[index].second;

    assert(ex.info(info_flags::integer));
    numeric num = ex_to<numeric>(ex);
    assert((num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS));

    debugLimitProblem("applying transformation rule (B), deleting " << ex
                      << " (" << InftyDirNames[dir] << ")");

    expressions.erase(expressions.begin() + index);

    dump("resulting limit problem");
}


void LimitProblem::removePolynomial(int index) {
    assert(index >= 0 && index < expressions.size());

    Expression ex = expressions[index].first;
    InftyDir dir = expressions[index].second;

    ExprSymbolSet variables = ex.getVariables();

    // ex has to be a univariate polynomial
    assert(ex.info(info_flags::polynomial));
    assert(variables.size() == 1);
    assert((dir == POS) || (dir == POS_INF) || (dir == NEG_INF));

    ExprSymbol var = *variables.begin();

    Expression expanded = ex.expand();
    debugLimitProblem("expanded " << ex << " to " << expanded);

    if (is_a<add>(expanded)) {
        Expression leadingTerm = expanded.lcoeff(var) * pow(var, expanded.degree(var));

        debugLimitProblem("the leading term is " << leadingTerm);

        if (dir == POS) {
            // Fix the direction of the expression
            expressions[index].second = POS_INF;
        }
        expressions[index].first = leadingTerm;

        debugLimitProblem("applying transformation rule (D), replacing " << ex
                      << " (" << InftyDirNames[dir] << ") by "
                      << leadingTerm << " (" << InftyDirNames[expressions[index].second] << ")");

    } else {
        debugLimitProblem(ex << "is already a monom");
    }

    dump("resulting limit problem");
}


bool LimitProblem::isSolved() const {
    // Check if an expression is not a variable
    for (const InftyExpression &ex : expressions) {
        if (!is_a<symbol>(ex.first)) {
            debugLimitProblem(ex.first << " is not a variable");

            return false;
        }
    }

    // Check if there is a variable with two different directions
    for (int i = 0; i < expressions.size(); i++) {
        for (int j = i + 1; j < expressions.size(); j++) {
            if (expressions[i].second != expressions[j].second
                && expressions[i].first.compare(expressions[j].first) == 0) {
                return false;
            }
        }
    }

    return true;
}


LimitProblem::LimitProblem(const std::vector<InftyExpression> &exps, int removeIndex) {
    assert(removeIndex >= 0 && removeIndex < exps.size());

    for (int i = 0; i < exps.size(); ++i) {
        if (i != removeIndex) {
            expressions.push_back(exps[i]);
        }
    }
}


void LimitProblem::dump(const std::string &description) const {
#ifdef DEBUG_LIMIT_PROBLEMS
    std::cout << description << ":" << std::endl;
    for (auto ex : expressions) {
        std::cout << ex.first << " (" << InftyDirNames[ex.second] << "), ";
    }
    std::cout << std::endl;

    std::cout << "the problem is " << (isSolved() ? "solved" : "not solved")
              << std::endl << std::endl;
#endif
}

LimitProblem LimitProblem::solve(const LimitProblem &problem) {
}