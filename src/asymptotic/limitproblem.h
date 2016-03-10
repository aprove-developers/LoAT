#ifndef LIMITPROBLEM_H
#define LIMITPROBLEM_H

#include <vector>
#include <utility>

#include "expression.h"
#include "guardtoolbox.h"

//TODO doc
class LimitProblem {
public:
    enum InftyDir { POS_INF, NEG_INF, POS_CONS, NEG_CONS, POS };
    static const char* InftyDirNames[];

    typedef std::pair<Expression, InftyDir> InftyExpression;

    LimitProblem(const GuardList &normalizedGuard,const Expression &cost);

    bool isSolved() const;

private:
    std::vector<InftyExpression> expressions;

private:
    //debug dumping
    void dump(const std::string &description) const;

public:
    static LimitProblem solve(const LimitProblem &problem);
};

#endif //LIMITPROBLEM_H