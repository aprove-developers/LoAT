#include "limitproblem.h"

bool LimitProblem::isSolved() const {
    // Check if an expression is not a variable
    for (const InftyExpression &ex : expressions) {
        ExprSymbolSet variables = ex.first.getVariables();

        if ((variables.size() != 1) || !ex.first.equalsVariable(*variables.cbegin())) {
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

LimitProblem LimitProblem::solve(const LimitProblem &problem) {
}