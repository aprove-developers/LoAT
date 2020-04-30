#ifndef LIMITSMT_H
#define LIMITSMT_H

#include "../expr/expression.hpp"
#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "limitproblem.hpp"

namespace LimitSmtEncoding {
    /**
     * Tries to solve the given limit problem by an encoding into a SMT query.
     * @returns the found solution (if any), the limit problem is not modified.
     */
    option<Subs> applyEncoding(const LimitProblem &currentLP, const Expr &cost, VarMan &varMan, Complexity currentRes, uint timeout);

    std::pair<Subs, Complexity> applyEncoding(const BoolExpr exp, const Expr &cost, VarMan &varMan, Complexity currentRes, uint timeout);
}

#endif //LIMITSMT_H
