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

#include "global.h"
#include "timing.h"

#include <z3++.h>
#include <vector>
#include <map>

class ITSProblem;
struct Transition;
class Expression;


/**
 * Wrapper around z3 context to allow convenient variable handling
 */
class Z3VariableContext : public z3::context {
public:
    enum VariableType { Integer, Real };

public:
    /**
     * Returns a variable of the given type with the given name if possible.
     * If a variable with given name and type does exist, it is returned.
     * If no variable with this name exists, a new one with given type is created.
     * If a variable with this name exists but is of a different type, a fresh variable
     * of the desired type with a modified name is created (same effect as getFreshVariable)
     */
    z3::expr getVariable(std::string name, VariableType type = Integer);

    /**
     * Like getVariable, but enforces that a new variable is created.
     * If newname is given, it is assigned the name of the fresh variable.
     */
    z3::expr getFreshVariable(std::string basename, VariableType type = Integer, std::string *newname = nullptr);

    /**
     * Returns true iff a variable of given name and type does already exist
     */
    bool hasVariable(std::string name, VariableType type = Integer) const;

private:
    bool isTypeEqual(const z3::expr &expr, VariableType type) const;

private:
    std::map<std::string,z3::expr> variables;
    std::map<std::string,int> basenameCount;
};


/**
 * Wrapper around z3 solver to gain timing information
 */
class Z3Solver : public z3::solver {
public:
    Z3Solver(Z3VariableContext &context) : z3::solver(context) {}
    inline z3::check_result check() {
        Timing::start(Timing::Z3);
        z3::check_result res = z3::solver::check();
        Timing::done(Timing::Z3);
        return res;
    }
};


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
    z3::expr concatExpressions(Z3VariableContext &context, const std::vector<z3::expr> &list, ConcatOperator op);

    /**
     * Given a z3 model, reads out the (real) value assigned to the given symbol and returns it as an Expression
     */
    Expression getRealFromModel(const z3::model &model, const z3::expr &symbol);

    /**
     * Returns the z3 result (sat/unsat/unknown) for the check if all expressions are satisfiable
     */
    z3::check_result checkExpressionsSAT(const std::vector<Expression> &list);

    /**
     * Extended version of checkExpressionsSAT that works on a given context and can be used to obtain the model
     * @note the model must have been created with the given context
     */
    z3::check_result checkExpressionsSAT(const std::vector<Expression> &list, Z3VariableContext &context, z3::model *model = nullptr);

    /**
     * Returns an approximation of the z3 result (sat/unsat/unknown) for the check if all expressions are satisfiable
     * @note currently, integer are treated as reals to reduce unknowns
     * @note using this function is *NOT* sound (obviously)
     */
    z3::check_result checkExpressionsSATapproximate(const std::vector<Expression> &list);

    /**
     * Returns true iff the implication "AND(lhs) -> rhs" is a (z3-provable) tautology in all occurring symbols
     */
    bool checkTautologicImplication(const std::vector<Expression> &lhs, const Expression &rhs);

    /**
     * Returns true iff the implication "AND(lhs) -> OR(AND(rhs))" is a (z3-provable) tautology in all occurring symbols
     */
    bool checkTautologicImplication(const std::vector<Expression> &lhs, const std::vector<std::vector<Expression>> &rhs);
}

#endif // Z3TOOLBOX_H
