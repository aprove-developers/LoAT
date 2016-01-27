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

#include <vector>
#include <map>

typedef std::vector<Expression> GuardList;

class ITRSProblem;

/**
 * Namespace for several helpers operating on relational expressions found in the guard
 */
namespace GuardToolbox {

    enum PropagationLevel { NoCoefficients=0, LinearCoefficients=1, Nonlinear=2 };
    enum PropagationFreevar { NoFreeOnRhs=0, AllowFreeOnRhs=1 };

    /**
     * Tries to remove equalities by propagating them into the other guard expressions
     * E.g. x == 2y, x > z can be transformed into 2y > z.
     *
     * @param level Defines in which cases a propagation is performed:
     * @param subs if given, this will be assigned the resulting substititon map
     * @param allowFunc if given, lambda must be true to propagate the given symbol
     *
     * @note non-free variables are never replaced by terms containing free variables.
     * Otherwise, for x==free, x might be removed, resulting in possible INF runtime, although free is bounded by x.
     *
     * NoCoefficients: the eliminated variable (x above) must have no coefficient (i.e. 1)
     * LinearCoefficients: a numeric coefficient is allowed
     * Nonlinear: Allow non-numeric coefficients (e.g. x*y == 2y^2 --> x == 2y), NOT SOUND!
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
    bool isValidGuard(const GuardList &guard);


    /**
     * Returns true iff all guard terms have polynomial rhs and lhs
     * @note guard must be a valid guard
     */
    bool isPolynomialGuard(const GuardList &guard, const GiNaC::lst &vars);


    /**
     * Given a relational, returns true iff this term is an equality
     */
    bool isEquality(const Expression &term);


    /**
     * Returns true iff term is a <,<=,>=,> relational with 2 arguments (not == or !=)
     */
    bool isValidInequality(const Expression &term);


    /**
     * Returns true iff term contains a free variable (note that this is possibly not very efficient)
     */
    bool containsFreeVar(const ITRSProblem &itrs, const Expression &term);

    /**
     * Given a valid inequality, replaces lhs and rhs with the given arguments and keeps operator
     * @return newly created Expression of the form lhs OP rhs (OP one of <,<=,>=,>).
     */
    Expression replaceLhsRhs(const Expression &term, Expression lhs, Expression rhs);


    /**
     * Returns true if term is a valid inequality and if rhs and lhs are linear in the given variables
     * (convenience function for isLinear and isValidInequality)
     */
    bool isLinearInequality(const Expression &term, const GiNaC::lst &vars);


    /**
     * Given a valid inequality, transforms it into only using the <= operator
     */
    Expression makeLessEqual(Expression term);


    /**
     * Given a <= inequality, moves all variables to lhs and constants to rhs
     */
    Expression splitVariablesAndConstants(const Expression &term);


    /**
     * Given a <= inequality, returns a <= inequality that represents the negated expression
     * (i.e. for lhs <= rhs, this is -lhs <= -rhs-1)
     */
    Expression negateLessEqualInequality(const Expression &term);


    /**
     * Given a <= inequality, returns true if lhs and rhs are numeric and this is a tautology,
     * or if lhs and rhs are the same (e.g. 0 <= 0 or 42 <= 127 or x <= x)
     */
    bool isTrivialInequality(const Expression &term);
}

#endif // GUARDTOOLBOX_H
