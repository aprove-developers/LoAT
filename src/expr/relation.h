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

#ifndef RELATION_H
#define RELATION_H

#include "global.h"
#include "expr/expression.h"
#include "util/option.h"

#include <vector>
#include <map>


/**
 * Namespace for several helpers operating on relational expressions.
 * Note that we never allow !=, so a relation uses one of the operators <,<=,==,>=,>.
 * Since Expression can already represent relationals, this is only a collection of functions and not a class.
 */
namespace Relation {

    /**
     * Checks whether ex is a relation, excluding the != operator
     */
    bool isRelation(const Expression &ex);

    /**
     * Checks whether ex is a relation where lhs and rhs are polynomial
     */
    bool isPolynomial(const Expression &ex);

    /**
     * Checks whether ex is a == relation
     */
    bool isEquality(const Expression &ex);

    /**
     * Checks whether ex is a <,<=,>=,> relation
     */
    bool isInequality(const Expression &ex);

    /**
     * Checks if ex is an inequality and if rhs and lhs are linear expressions
     * (convenience function for isLinear and isInequality)
     */
    bool isLinearInequality(const Expression &ex);

    /**
     * Checks whether ex is of the form "term > 0"
     */
    bool isGreaterThanZero(const Expression &ex);

    /**
     * Checks if ex is a <= inequality (less or equal)
     */
    bool isLessOrEqual(const Expression &ex);

    /**
     * Given a relation, replaces lhs and rhs with the given arguments and keeps operator
     * @return newly created Expression of the form lhs OP rhs (OP one of <,<=,=,>=,>).
     */
    Expression replaceLhsRhs(const Expression &rel, Expression lhs, Expression rhs);

    /**
     * Given an inequality, transforms it into one only using the <= operator
     * @note assumes integer arithmetic to translate < to <=
     */
    Expression toLessEq(Expression rel);

    /**
     * Given an inequality, transforms it into one only using the > operator
     * @note assumes integer arithmetic to translate e.g. >= to >
     */
    Expression toGreater(Expression rel);

    /**
     * Given an inequality, transforms it into one of the form lhs > 0
     * @note assumes integer arithmetic to translate e.g. >= to >
     */
    Expression normalizeInequality(Expression rel);

    /**
     * Given an inequality using the operator > or >=,
     * transforms it into one using the operator < or <=.
     * Does not change equations or inequalities using the operator < or <=.
     */
    Expression toLessOrLessEq(Expression rel);

    /**
     * Given a relation, moves all variables to lhs and constants to rhs
     */
    Expression splitVariablesAndConstants(const Expression &rel);

    /**
     * Given a <= inequality, returns a <= inequality that represents the negated expression
     * (i.e. for lhs <= rhs, this is -lhs <= -rhs-1)
     * @note this assumes that lhs, rhs are integer valued (so lhs > rhs can be rewritten as lhs >= rhs+1)
     */
    Expression negateLessEqInequality(const Expression &relLessEq);

    /**
     * Given any relation, checks if the relation is trivially true or false,
     * by checking if rhs-lhs is a numeric value. If unsure, returns none.
     * @return true/false if the relation is trivially valid/invalid
     */
    option<bool> checkTrivial(const Expression &rel);

    /**
     * Wrapper around checkTrivial() to check if a given relation is trivially true.
     * @return true if the relation is a tautology, false has no meaning
     */
    bool isTriviallyTrue(const Expression &rel);
}

#endif // RELATION_H
