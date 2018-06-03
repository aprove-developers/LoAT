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

#include "its/rule.h"
#include "its/variablemanager.h"



/**
 * This class is the interface end to the recurrence solver PURRS,
 * and allows calculating iterated cost and update
 */
class Recurrence
{
private:

public:
    /**
     * Iterates the rule's update and cost, similar to iterateUpdateCost.
     * In addition to iterateUpdateCost, an additional heuristic is used if no dependency order is found.
     * This heuristic adds new constraints to the rule's guard and is thus only used in this method.
     */
    static bool iterateRule(const VarMan &varMan, LinearRule &rule, const Expression &metering);

    /**
     * Tries to solve recurrences to compute the iterated update and cost.
     * If successful, returns true and modifies update and cost to represent update/cost after N iterations.
     * @return true iff both computations were successful
     */
    static bool iterateUpdateAndCost(const VarMan &varMan, UpdateMap &update, Expression &cost, const Expression &N);

private:
    Recurrence(const VarMan &varMan, const std::vector<VariableIdx> &dependencyOrder);

    /**
     * Main implementation
     */
    bool iterateAll(UpdateMap &update, Expression &cost, const Expression &metering);

    /**
     * Computes the iterated update, with meterfunc as iteration step (if possible).
     * @note dependencyOrder must be set before
     * @note sets updatePreRecurrences
     */
    boost::optional<UpdateMap> iterateUpdate(const UpdateMap &update, const Expression &meterfunc);

    /**
     * Computes the iterated cost, with meterfunc as iteration step (if possible).
     * @note updatePreRecurrences must be set before (so iterateUpdate() needs to be called before)
     */
    boost::optional<Expression> iterateCost(const Expression &cost, const Expression &meterfunc);

    /**
     * Helper for iterateUpdate.
     * Tries to find a recurrence for the given single update.
     * Note that all variables occurring in update must have been solved before (and added to updatePreRecurrences).
     */
    boost::optional<Expression> findUpdateRecurrence(const Expression &updateRhs, ExprSymbol updateLhs);

    /**
     * Tries to find a recurrence for the given cost term.
     * Note that all variables occuring in update must have been solved before (and added to updatePreRecurrences).
     */
    boost::optional<Expression> findCostRecurrence(Expression cost);

private:
    /**
     * To query variable names/indices
     */
    const VariableManager &varMan;

    /**
     * Purrs::Recurrence::n converted to a ginac expression, for convenience only
     */
    const Expression ginacN;

    /**
     * Order in which recurrences for updated variables can be computed
     */
    std::vector<VariableIdx> dependencyOrder;

    /**
     * Substitution map, mapping variables to their recurrence equations
     * @note the recurrence equations are valid *before* the transition is taken,
     * i.e. these are the terms for r(n-1) and _not_ for r(n) where r is the recurrence equation.
     */
    GiNaC::exmap updatePreRecurrences;
};

#endif // RECURRENCE_H