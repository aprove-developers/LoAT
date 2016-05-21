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

#ifndef FARKAS_H
#define FARKAS_H

#include "global.h"

#include "expression.h"
#include "z3toolbox.h"
#include "itrs/itrs.h"

#include <vector>
#include <map>

struct Transition;

/**
 * Class to encapsulate the process of finding a metering function for a given transition using Z3 and Farkas lemma
 *
 * Central constraints for the metering function f (G is guard, U is update, x the variables)
 *  (1)  (not G)   implies  f(x) <= 0
 *  (2)  G         implies  f(x) >= 1 (equivalent to f(x) > 0)
 *  (3)  (G and U) implies  f(x) <= f(x') + 1
 *
 * Farkas Lemma:
 *  Forall x: (A*x <= b implies c*x <= delta) can be rewritten as:
 *  Exists l: l >= 0, l^T * A = c^T, l^T * b <= delta.
 * We use x as the variables, A and b to represent guard/update, c as coefficients for the linear metering polynomial.
 */
class FarkasMeterGenerator
{
public:
    /**
     * Success: metering function was found
     * Unbounded: the loop can be executed unbounded (there is no limiting guard)
     * Nonlinear: the problem is nonlinar and could not be substituted to a linear problem
     * ConflictVar: two variables are limiting the execution of the loop, we would need min(A,B) or max(A,B) to resolve
     * Unsat: no metering function was found (z3 unknown/unsat)
     */
    enum Result { Success, Unbounded, Nonlinear, ConflictVar, Unsat };

    /**
     * Try to find a metering function for the given transition in the given ITS
     * @param its the ITRSProblem instance, providing lists of all symbols
     * @param t the Transition to find a metering function for, NOTE: modified by instantiation
     * @param result the resulting metering function (output only)
     * @param conflictVar if given, this is set if it would help to add A > B (or B > A) to the guard for variables A,B
     * @return Success iff a metering function was found and result was set, otherwise indicates type of failure
     *
     * @note the transition t might be modified (by freevar instantiation) only if the result is Success
     */
    static Result generate(ITRSProblem &its, Transition &t, Expression &result, std::pair<VariableIndex, VariableIndex> *conflictVar = nullptr);

    /**
     * Prepares the guard to get better farkas results by adding additional constraints
     * @return true iff the transition was changed
     */
    static bool prepareGuard(ITRSProblem &its, Transition &t);

private:
    FarkasMeterGenerator(ITRSProblem &its, const Transition &t);

    /**
     * Some preprocessing steps as equality propagation and elimination by transitive closure
     * to remove as many free variables as possible. Modifies guard,update.
     * @note the current implementation relies on reduceGuard() and findRelevantVariables(), but discards their changes.
     */
    void preprocessFreevars();

    /**
     * Modifies guard (member) to contain only <,<=,>=,> by replacing == with <= and >=
     * @return true iff successfull, false if guard contains != which cannot be handled
     */
    bool makeRelationalGuard();

    /**
     * Sets reducedGuard (member) to contain only the constraints from guard (member)
     * which are relevant for the metering function (contains an updated variable and isnt always true for the update)
     * (e.g. in n >= 0, i >= 0, i < n with i=i+1, the constraints n >= 0 and i >= 0 are not relevant)
     *
     * Sets irrelevantGuard (member) to contain exactly the contraints which were dropped for the reducedGuard.
     */
    void reduceGuard();

    /**
     * Sets varlist and symbols (members) to contain the variables that might occur in the metering function.
     *
     * A variable is relevant iff
     *  a) it appears in reduced guard (and thus might influence the rank func)
     *  b) it appears on update rhs, where the lhs appears in any guard (indirect influence)
     * In other cases, the variable is irrelevant for the metering function.
     */
    void findRelevantVariables();

    /**
     * Tiny helper. Returns true iff vi is a relevant variable, i.e. is contained in varlist
     * @note findRelevantVariables must be called before
     * @note this uses linear search in varlist and is thus potentially slow
     */
    bool isRelevantVariable(VariableIndex vi) const;

    /**
     * Removes updates that do not update a relevant var; removes conditions from the guard
     * (and the reducedGuard) that do not contain any relevant variables.
     * Modifies update,guard,reducedGuard (members).
     */
    void restrictToRelevantVariables();

    /**
     * Helper function to make a given term linear by simple subtitutions
     * @param term The (possibly nonlinear) expression [input and output]
     * @param vars The variables term should be linear in
     * @param subsVars Set of all variables for which powers/multiplications of them are substituted (e.g. x if x^2 --> z was substituted) [input and output]
     * @param subsMap The resulting substitution. Must be applied to term before calling! [input and output]
     * @return true iff a substitution was found and term is now linear
     */
    bool makeLinear(Expression &term, ExprList vars, ExprSymbolSet &subsVars, GiNaC::exmap &subsMap);

    /**
     * Modifies guard and update to be linear if possible
     * @note the reverse substitution is stored in nonlinearSubs
     * @note As this might change guard/update, relevant variables need to be recalculated afterwards
     * @return true iff a aubstitution was found and guard and update are now linear expressions
     */
    bool makeLinearTransition();

    /**
     * Builds the required lists of constraints (guard, reducedGuard, guardUpdate)
     * in the form "linear term <= constant"
     */
    void buildConstraints();

    /**
     * Creates z3 symbols for the coefficients for all relevant variables
     */
    void createCoefficients(Z3VariableContext::VariableType type);

    /**
     * Applies farkas lemma to transform the given constraints into z3 constraints
     * @param constraints Of the form "linear term <= constant", such that they can be written as "A * x <= b"
     * @param vars List of variables "x"
     * @param coeff A z3 symbol representing the coefficient for every variable (same size as vars)
     * @param c0 The z3 symbol for the absolute coefficient
     * @param delta integer value such that "A * x <= b" implies "coefficients * x <= delta"
     * @return the resulting z3 expression without all quantors
     */
    z3::expr applyFarkas(const std::vector<Expression> &constraints, const std::vector<ExprSymbol> &vars, const std::vector<z3::expr> &coeff, z3::expr c0, int delta) const;

    /**
     * Helper to build the implication: "(G and U) --> f(x)-f(x') <= 1" using applyFarkas
     */
    z3::expr genUpdateImplication() const;

    /**
     * Helper to build the implication: "(not G) --> f(x) <= 0" using multiple applyFarkas calls (which are AND-concated)
     * @note makes use of reducedGuard instead of guard
     */
    z3::expr genNotGuardImplication() const;

    /**
     * Helper to build the implication: "G --> f(x) > 0" using applyFarkas
     * @param strict if true, the rhs is strict, i.e. f(x) > 0 formulated as f(x) >= 1; if false f(x) >= 0 is used
     */
    z3::expr genGuardPositiveImplication(bool strict) const;

    /**
     * Helper to build constarints to suppress trivial solutions, i.e. "OR c_i != 0" for the coefficients c_i
     */
    z3::expr genNonTrivial() const;

    /**
     * Given the z3 model, builds the corresponding linear metering function and applies the reverse substitution nonlinearSubs
     */
    Expression buildResult(const z3::model &model) const;

    /**
     * Creates all combinations of instantiating free variables by their bounds (i.e. free <= x --> set free=x)
     * @return list of all possible combinations (limited by FREEVAR_INSTANTIATE_MAXBOUNDS per variable).
     */
    std::stack<GiNaC::exmap> instantiateFreeVariables() const;

private:
    /**
     * The ITRSProblem instance, used for list of variables and handling of fresh symbols
     */
    ITRSProblem &its;

    /**
     * The transition data (possible modified by makeLinearTransition)
     */
    UpdateMap update;
    std::vector<Expression> guard;

    /**
     * The transition's guard without irrelevant constraints
     */
    std::vector<Expression> reducedGuard;

    /**
     * The irrelevant constraints from the transition's guard (which were dropped for reducedGuard)
     */
    std::vector<Expression> irrelevantGuard;


    /**
     * Reverse substitution for nonlinear guard/updates
     */
    GiNaC::exmap nonlinearSubs;

    /**
     * The Z3 context to handle z3 symbols/expressions
     */
    mutable Z3VariableContext context;

    /**
     * List of all variables that are relevant and thus occur in the metering function
     */
    std::vector<VariableIndex> varlist;

    /**
     * Z3 symbols for the metering polynomial coefficients (absolute and per-variable)
     * @note coefficients are only created for relevant variables
     */
    z3::expr coeff0;
    std::vector<z3::expr> coeffs; //shares index with varlist

    /**
     * Corresponding ginac symbol for ever entry in varlist
     */
    std::vector<ExprSymbol> symbols; //shares index with varlist

    /**
     * Maps relevant variables to a ginac symbol representing the updated (primed) variable
     */
    std::map<VariableIndex,ExprSymbol> primedSymbols; //only relevant variables (from varlist)

    /**
     * Linear constraints (of the form "linear term <= constant") obtained from guard, reduced guard, irrelevant guard, guard and update.
     */
    struct {
        std::vector<Expression> guard, reducedGuard, irrelevantGuard, guardUpdate;
    } constraints;
};

#endif // FARKAS_H
