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

#ifndef RECURRENCE_H
#define RECURRENCE_H

#include "global.h"
#include "its.h"

struct Transition;


/**
 * This class is the interface end to the recurrence solver PURRS,
 * and allows calculating iterated cost and update
 */
class Recurrence
{
private:

public:
    //Updates trans.update and trans.cost, but only if result is true, otherwise unchanged
    /**
     * Tries to solve recurrences for the iterated update and cost.
     * If successful, returns true and modifies trans to contain the iterated update and cost (using the given metering function as "iteration step")
     * Otherwise, false is returned and trans is left unchanged
     */
    static bool calcIterated(const ITSProblem &its, Transition &trans, const Expression &meterfunc);

private:
    Recurrence(const ITSProblem &its);

    /**
     * Returns true iff iterated update was calculated successfully and newUpdate has been set accordingly (with runtime as "iteration step")
     */
    bool calcIteratedUpdate(const UpdateMap &oldUpdate, const Expression &meterfunc, UpdateMap &newUpdate);

    /**
     * Returns true iff iterated cost was calculated successfully and newCost has been set (with meterfunc as "iteration step")
     * @note calcIteratedUpdate *must* be called before, as this relies on the recurrences found there
     */
    bool calcIteratedCost(const Expression &cost, const Expression &meterfunc, Expression &newCost);

private:
    /**
     * Tries to find an order to calculate recurrence equations.
     * If this is not possible, update is modified and addGuard is set (heuristically assume that all problematic variables have the same value)
     * @return list indicating the order
     */
    std::vector<VariableIndex> dependencyOrder(UpdateMap &update);

    /**
     * Tries to find a recurrence for the given update (target is the update's lhs).
     * Note that all variables occuring in update must have been solved already.
     * @return true if successful and result has been set
     */
    bool findUpdateRecurrence(Expression update, ExprSymbol target, Expression &result);

    /**
     * Tries to find a recurrence for the given cost term.
     * Note that all variables occuring in update must have been solved already.
     * @return true if successful and result has been set
     */
    bool findCostRecurrence(Expression cost, Expression &result);

private:
    /**
     * The ITS data, to query variable names/indices
     */
    const ITSProblem &its;

    /**
     * Purrs::Recurrence::n converted to a ginac expression, for convenience only
     */
    const Expression ginacN;

    /**
     * Additional constraints that have to be added to the guard.
     * This is used to solve recurrences which cannot be ordered, e.g. A=B+1, B=A+1.
     * if we then add A==B to the guard, we can instead use A=A+1, B=A+1
     */
    std::vector<Expression> addGuard;

    /**
     * Substitution map, mapping variables to their recurrence equations
     * @note the recurrence equations are valid *before* the transition is taken,
     * i.e. these are the terms for r(n-1) and _not_ for r(n) where r is the recurrence equation.
     */
    GiNaC::exmap knownPreRecurrences;
};

#endif // RECURRENCE_H
