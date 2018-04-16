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

#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "its/variablemanager.h"

struct Transition;

/**
 * Methods useful for preprocessing / simplifying the transitions
 */
namespace Preprocess
{
    /**
     * Removes the last constraint from the guard if it is already implied by the other constraints.
     * The last constraint is usually "cost >= 0" (to ensure user-given costs are nonnegative).
     * @note This relies on the parsed adding "cost >= 0" as last constraint to the guard.
     * @return true iff "cost >= 0" is implied by the guard and was removed
     */
    bool tryToRemoveCost(GuardList &guard);

    /**
     * Expensive preprocessing of the given transition.
     * This includes finding equalities, removing free variables, removing trivial constraints.
     * @param trans the transition, modified.
     * @return true iff trans was modified
     */
    bool simplifyTransition(const VarMan &varMan, Transition &trans);

    /**
     * Removes trivial terms from the given guard, i.e. 42 <= 1337 or x <= x+1
     * @note this does _not_ involve any SMT queries and thus only removes very trivial terms
     * @return true iff guard was modified
     */
    bool removeTrivialGuards(GuardList &guard);

    /**
     * Removes terms for which stronger variants appear in the guard, i.e. x >= 0, x > 0 --> x > 0
     * @note this _does_ involve many SMT queries (though only for every pair, transitivity is not checked)
     * @return true iff guard was modified
     */
    bool removeWeakerGuards(GuardList &guard);

    /**
     * Removes trivial updates of the form x <- x.
     * @return true iff update was modified
     */
    bool removeTrivialUpdates(const VarMan &varMan, UpdateMap &update);

    /**
     * Expensive preprocessing step to remove all free variables from the update and,
     * where possible, also from the guard.
     * @param trans the transition, modified.
     * @return true iff trans was modified
     */
    bool eliminateFreeVars(const VarMan &varMan, Transition &trans);
}

#endif // PREPROCESSOR_H
