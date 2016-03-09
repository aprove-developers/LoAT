#ifndef LIMITPROBLEM_H
#define LIMITPROBLEM_H

#include <vector>
#include <utility>

#include "expression.h"

//TODO doc
class LimitProblem {
public:
    enum InftyDir { POS, NEG, POSCONS, NEGCONS };
    typedef std::pair<Expression, InftyDir> InftyExpression;

    bool isSolved() const;

private:
    std::vector<InftyExpression> expressions;

public:
    static LimitProblem solve(const LimitProblem &problem);
};

#endif //LIMITPROBLEM_H