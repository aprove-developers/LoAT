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

#ifndef LINEARIZE_H
#define LINEARIZE_H

#include "../../expr/expression.hpp"
#include "../../its/variablemanager.hpp"
#include "../../util/option.hpp"


/**
 * Linearize a rule's guard and update by substituting nonlinear expressions.
 *
 * E.g. "x^2 + y > 5" with update "y := a*b" becomes "x2 + y > 5" and "y := ab",
 * where "x2" and "ab" are fresh variables.
 *
 * Note that substituting an expression like "x^2" or "a*b" is only possible if
 * the variables (x,a,b) are not updated and do not occur in any other expressions.
 * E.g. "x^2 < x^3" cannot be substituted to "x2 < x3"
 * (since this would lose the relation between x2 and x3).
 *
 * Note that we do not care about the rule's cost, since linearization is only
 * a temporary step to make Farkas lemma applicable when finding metering functions.
 */
class Linearize {
public:
    /**
     * Modifies guard and update to be linear (if possible)
     * by substituting nonlinear expressions with fresh variables.
     * Requires guard to only contain inequalities.
     * Returns the reverse substitution, if linearization was successful.
     */
    static option<ExprMap> linearizeGuardUpdates(VarMan &varMan, GuardList &guard, std::vector<Subs> &updates);

private:
    Linearize(GuardList &guard, std::vector<Subs> &updates, VarMan &varMan)
        : guard(guard), updates(updates), varMan(varMan) {}

    /**
     * Tries to add all monomials of the given expression to the given set.
     * A monomial is a product of variable powers, e.g. x, x^2, x*y etc.
     *
     * Fails if a nonlinear term is too complicated, e.g., if ex is non-polynomial
     * or contains an expression like y*x^2 (we only handle x^n) or x*y*z (we only handle x*y).
     *
     * @return true if successful (all nonlinear subterms have been added to the set)
     */
    static bool collectMonomials(const Expr &ex, ExprSet &monomials);

    /**
     * Collects all monomials of the guard via collectMonomials
     */
    bool collectMonomialsInGuard(ExprSet &monomials) const;

    /**
     * Collects all monomials of all updates via collectMonomials
     */
    bool collectMonomialsInUpdates(ExprSet &monomials) const;

    /**
     * Checks if any substitutions need to be performed,
     * i.e., if there is any nonlinear monomial in the given set.
     */
    bool needsLinearization(const ExprSet &monomials) const;

    /**
     * Checks if it is safe to replace all nonlinear monomials of the given set.
     * This is the case if each variable occurs in at most one monomial,
     * and if no term contains an updated variable.
     *
     * For instance, we cannot replace x^2 if x (or x^3 or x*y) also occurs somewhere.
     *
     * @return true if the replacement is safe
     */
    bool checkForConflicts(const ExprSet &monomials) const;

    /**
     * Constructs a substitution (on terms, not on variables, e.g., x^2/z)
     * that maps every term of the given set to an individual fresh variable.
     *
     * Note that fresh variables are non-temporary, even if the original variable
     * was a temporary variable.
     *
     * If an even power is substituted (e.g. x^2/z), a constraint is added
     * to additionalGuard to remember that its value is non-negative (e.g. z >= 0).
     *
     * @return The constructed substitution (map from terms to variables)
     */
    ExprMap buildSubstitution(const ExprSet &nonlinearTerms);

    /**
     * Applies the given substitution to the entire guard and update.
     * Takes care of the unintuitive behaviour of GiNaC::subs
     */
    void applySubstitution(const ExprMap &subs);

    /**
     * Computes the reverse substitution of the given mapping.
     */
    static ExprMap reverseSubstitution(const ExprMap &subs);


private:
    // Guard and updates of the rule, only modified by applying substitutions.
    GuardList &guard;
    std::vector<Subs> &updates;

    // Additional constraints to be added to the resulting guard.
    // They retain information that is lost durng substitution, e.g. that x^2 is always nonnegative.
    GuardList additionalGuard;

    // For fresh variables
    VariableManager &varMan;
};

#endif
