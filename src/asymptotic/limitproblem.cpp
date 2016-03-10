#include "limitproblem.h"

#include "debug.h"

using namespace GiNaC;

const char* LimitProblem::InftyDirNames[] = { "+", "-", "+!", "-!", "+/+!"};

LimitProblem::LimitProblem(const GuardList &normalizedGuard,const Expression &cost) {
    for (const Expression &ex : normalizedGuard) {
        assert(GuardToolbox::isNormalizedInequality(ex));

        expressions.push_back(InftyExpression(ex.lhs(), POS));
    }

    assert(!is_a<relational>(cost));
    expressions.push_back(InftyExpression(cost, POS_INF));

    dump("Created initial limit problem");
    debugLimitProblem("the problem is " << (isSolved() ? "solved" : "not solved") << std::endl);
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

void LimitProblem::dump(const std::string &description) const {
#ifdef DEBUG_LIMIT_PROBLEMS
    std::cout << description << ":" << std::endl;
    for (auto ex : expressions) {
        std::cout << ex.first << " (" << InftyDirNames[ex.second] << "), ";
    }
    std::cout << std::endl;
#endif
}

LimitProblem LimitProblem::solve(const LimitProblem &problem) {
}