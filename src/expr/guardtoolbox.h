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

#ifndef GUARDTOOLBOX_H
#define GUARDTOOLBOX_H

#include "global.h"
#include "expression.h"
#include "its/itsproblem.h"

#include <vector>
#include <map>

// TODO: Port to new ITSProblem
class ITRSProblem;

/**
 * Namespace for several functions operating on guards (list of relational expressions) and related helpers.
 * Note: We never allow != in relations.
 */
namespace GuardToolbox {

    enum PropagationLevel { NoCoefficients=0, LinearCoefficients=1, Nonlinear=2 };
    enum PropagationFreevar { NoFreeOnRhs=0, AllowFreeOnRhs=1 };

    /**
     * Tries to remove equalities by propagating them into the other guard expressions
     * E.g. x == 2y, x > z can be transformed into 2y > z.
     *
     * @param level Defines in which cases a propagation is performed:
     * @param freevar Defines if replacing non-free variables by free variables is allowed
     * @param subs if given, this will be assigned the resulting substititon map
     * @param allowFunc if given, lambda must be true to propagate the given symbol
     *
     * @note is is ensured that substitutions of the form x/x^2 cannot happen
     *
     * @note replacing non-free variables by free variables is not sound for the runtime complexity
     * (only if the free variable is somehow marked as bounded afterwards)
     * Example: for x==free, x might be substituted by free, resulting in possible INF runtime, although free was in fact bounded by x.
     *
     * NoCoefficients: the eliminated variable (x above) must have no coefficient (i.e. 1)
     * LinearCoefficients: a numeric coefficient is allowed, NOT SOUND IN MOST CASES!
     * Nonlinear: Allow non-numeric coefficients (e.g. x*y == 2y^2 --> x == 2y), NOT SOUND IN ALMOST ALL CASES!
     *
     * @return true if any progpagation was performed.
     */
    bool propagateEqualities(const ITRSProblem &itrs, GuardList &guard, PropagationLevel level, PropagationFreevar freevar, GiNaC::exmap *subs = nullptr,
                             std::function<bool(const ExprSymbol &)> allowFunc = [](const ExprSymbol &){return true;});


    /**
     * Tries to replace inequalities using thir transitive closure
     * E.g. A <= x and x <= B will be replaced by A <= B.
     * Note that for soundness, all terms with x must be replaced at once.
     * Note that x may not have any coefficient in any of these terms.
     *
     * @note this is only sound for the resulting runtime, if only free variables are allowed to be eliminated!
     *
     * @param vars all variable symbols that may appear in the guard
     * @param removeHalfBounds if true, terms like a <= x (without x <= b) will also be removed!
     * @param allowFunc if lambda is false, the given variable may not be considered for elimination.
     *
     * @return true if any changes have been made
     */
    bool eliminateByTransitiveClosure(GuardList &guard, const GiNaC::lst &vars, bool removeHalfBounds,
                                      std::function<bool(const ExprSymbol &)> allowFunc);


    /**
     * Tries to solve the equation term == 0 for the given variable, using the given level of restrictiveness
     * @note term must be polynomial. It must _not_ be a relational expression
     * @param term both input and output
     * @return true if successful and term contains the result. False otherwise (term is left unchanged!)
     */
    bool solveTermFor(Expression &term, const ExprSymbol &var, PropagationLevel level);


    /**
     * Replaces bidirectional inequalities, e.g. x <= y, y >= x by an equality, e.g. x == y
     * @note expensive for large guards
     * @return true iff guard was changed. The inequalties are rmoved, the equality is added to guard
     */
    bool findEqualities(GuardList &guard);


    /**
     * Returns true iff all guard terms are relational without the use of !=
     */
    bool isWellformedGuard(const GuardList &guard);


    /**
     * Returns true iff all guard terms have polynomial rhs and lhs
     * @note guard must be a well-formed guard
     */
    bool isPolynomialGuard(const GuardList &guard, const GiNaC::lst &vars);


    /**
     * Returns true iff term contains a free variable (note that this is possibly not very efficient)
     */
    bool containsFreeVar(const ITRSProblem &itrs, const Expression &term);


    /**
     * Compose two substitutions, i.e. compute f ∘ g ("f after g")
     */
    GiNaC::exmap composeSubs(const GiNaC::exmap &f, const GiNaC::exmap &g);
}


#endif // GUARDTOOLBOX_H