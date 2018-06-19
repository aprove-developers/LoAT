/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#ifndef Z3TOOLBOX_H
#define Z3TOOLBOX_H

#include <z3++.h>
#include <vector>

class Expression;
class Z3Context;


/**
 * Namespace for several helpers to access z3
 */
namespace Z3Toolbox {
    enum ConcatOperator { ConcatAnd, ConcatOr };

    /**
     * Tiny helper to create a list of and- or or-concated expressions
     * @param list expressions to concat
     * @param op the operator to use
     * @return the resulting expression
     */
    z3::expr concat(Z3Context &context, const std::vector<z3::expr> &list, ConcatOperator op);

    /**
     * Given a z3 model, reads out the (real) value assigned to the given symbol and returns it as an Expression
     */
    Expression getRealFromModel(const z3::model &model, const z3::expr &symbol);

    /**
     * Calls z3 for the conjunction of all given expressions, returns the result (sat/unsat/unknown).
     */
    z3::check_result checkAll(const std::vector<Expression> &list);

    /**
     * Extended version of checkAll that works on a given context and can be used to obtain the model
     * @note the model must have been created with the given context
     */
    z3::check_result checkAll(const std::vector<Expression> &list, Z3Context &context, z3::model *model = nullptr);

    /**
     * Returns an approximation of the z3 result (sat/unsat/unknown) for the check if all expressions are satisfiable
     * @note currently, integer are treated as reals to reduce unknowns and exponential expressions are skipped
     * @note using this function is *NOT* sound (since it is only an approximation)
     */
    z3::check_result checkAllApproximate(const std::vector<Expression> &list);

    /**
     * Returns true iff the implication "AND(lhs) -> rhs" is a (z3-provable) tautology in all occurring symbols
     */
    bool isValidImplication(const std::vector<Expression> &lhs, const Expression &rhs);
}

#endif // Z3TOOLBOX_H
