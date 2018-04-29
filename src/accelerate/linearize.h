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

#include "global.h"

#include "expr/expression.h"
#include "z3/z3context.h"
#include "its/variablemanager.h"

#include <vector>
#include <map>


#include "flowgraph.h" // only for Transition

struct FarkasTrans {
    GuardList guard;
    UpdateMap update;
    Expression cost;
};


std::ostream& operator<<(std::ostream &s, const FarkasTrans &trans);


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
    boost::optional<GiNaC::exmap> linearizeGuardUpdate(GuardList &guard, UpdateMap &update, VarMan &varMan);

private:
    Linearize(GuardList &guard, UpdateMap &update, VarMan &varMan) : guard(guard), update(update), varMan(varMan) {}

    /**
     * Checks if we can substitute the given expression by a fresh variable (with the given name).
     * If applicable, updates subsMap and subsVars accordingly.
     */
    bool substituteExpression(const Expression &ex, std::string name);

    /**
     * Tries to linearize the given expression.
     * If possible, modifies the given term and subsVar, subsMap.
     * Might also extend guard (to keep information that is lost when substituting).
     *
     * @return true iff linearization was successful
     */
    bool linearizeExpression(Expression &term);

    /**
     * Tries to linearize guard.
     * If possible, modifies guard, subsVar, subsMap.
     */
    bool linearizeGuard();

    /**
     * Tries to linearize update.
     * If possible, modifies update, subsVar, subsMap.
     * Might also modify guard (see linearizeExpression).
     */
    bool linearizeUpdate();

    /**
     * Applies the computed substitution subsMap to the entire guard and update.
     */
    void applySubstitution();

    /**
     * Computes the reverse substitution of subsMap
     */
    GiNaC::exmap reverseSubstitution() const;

private:
    // The set of all variables occurring in substituted expressions.
    // If we substitute x^2/z, then x is added to this set.
    ExprSymbolSet subsVars;

    // The substitution of nonlinear expression, e.g. x^2/z.
    // Note that this is not a substitution of variables, but of expressions.
    GiNaC::exmap subsMap;

    // Guard and update of the rule, may both be modified
    // (by substituting nonlinear expressions; guard can also be extended)
    GuardList &guard;
    UpdateMap &update;

    // For fresh variables
    VariableManager &varMan;
};

#endif // LINEARIZE_H
