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