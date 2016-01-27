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
#include "itrs.h"

struct Transition;


class Recurrence
{
private:

public:
    //Updates trans.update and trans.cost, but only if result is true, otherwise unchanged
    static bool calcIterated(const ITRSProblem &itrs, Transition &trans, const Expression &rankfunc);

private:
    Recurrence(const ITRSProblem &itrs);

    //returns true iff success and update was updated; otherwise false and everything is left unchanged
    //! @note might modify update when the recurrence is too hard to solve otherwise (cannot be ordered)
    bool calcIteratedUpdate(UpdateMap &update, const Expression &runtime, UpdateMap &newUpdate);

    //returns true if everything was successful and cost was updated, otherwise nothing is changed
    //! @note calcIteratedUpdate *must* be called before!
    bool calcIteratedCost(const Expression &cost, const Expression &rankfunc, Expression &newCost);

private:
    std::vector<VariableIndex> dependencyOrder(UpdateMap &update);
    bool findUpdateRecurrence(Expression update, ExprSymbol target, Expression &result);
    bool findCostRecurrence(Expression cost, Expression &result);

private:
    /**
     * The ITRS data, to query variable names/indices
     */
    const ITRSProblem &itrs;

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
