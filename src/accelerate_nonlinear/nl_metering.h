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

#ifndef NL_METERING_H
#define NL_METERING_H

#include "expr/expression.h"
#include "z3/z3context.h"
#include "its/variablemanager.h"
#include "its/rule.h"

#include <vector>
#include <map>


/**
 * Class to encapsulate the process of finding a metering function for a given transition using z3 and Farkas lemma
 *
 * Central constraints for the metering function f (G is guard, U is update, x the variables)
 *  (1)  (not G)   implies  f(x) <= 0
 *  (2)   G        implies  f(x) >= 1 (equivalent to f(x) > 0 on integers)
 *  (3)  (G and U) implies  f(x) <= f(x') + 1
 *
 * The search is performed by constructing a linear template f(x) = c*x + c0,
 * where x are variables (only those we consider relevant for the metering function)
 * and c are the corresponding coefficients (c0 is the absolute coefficient).
 * The values for c and c0 are determined by a z3 query, such that constraints (1)-(3) are satisfied.
 */
class MeteringFinderNL {
public:
    /**
     * Success: metering function was found
     * Unbounded: the loop can be executed unbounded (there is no limiting guard)
     * Nonlinear: the problem is nonlinear and could not be substituted to a linear problem
     * ConflictVar: two variables are limiting the execution of the loop, we would need min(A,B) or max(A,B) to resolve
     * Unsat: no metering function was found (z3 unknown/unsat)
     */
    enum ResultKind { Success, Unbounded, Nonlinear, ConflictVar, Unsat };

    struct Result {
        // Flag indicating whether a metering function was successfully found
        ResultKind result;

        // The metering function (only relevant if result is Success)
        Expression metering;

        // The pair of conflicting variables (only relevant if result is ConflictVar)
        VariablePair conflictVar;

        // Additional constraint that has to be added to the rule's guard to ensure correctness.
        // Only relevant if result is Success (and real coefficients are used).
        boost::optional<Expression> integralConstraint;
    };

    /**
     * Tries to find a metering function for the given rule by identifying relevant variables,
     * constructing a template for the metering function using these variables and then using
     * z3 to fill the template given encodings of the constraints (1), (2) and (3).
     *
     * @return The resulting metering function or the reason for failure (and possibly additional information).
     */
    static Result generate(VarMan &varMan, const NonlinearRule &rule);

    /**
     * Heuristic to instantiate temporary variables by their bounds (e.g. for "x <= 4", instantiate x by 4).
     * This might help to find a metering function, but of course makes the rule less general.
     *
     * This method tries several instantiations, until it finds one which satisfies the main constraints
     * for the metering function (so calling generate with the resulting rule is likely successful).
     * This is very expensive!
     *
     * @return true iff instantiation was successful (and rule has been modified accordingly)
     */
//    static bool instantiateTempVarsHeuristic(VarMan &varMan, LinearRule &rule); // TODO: adapt this to nonlinear rule

private:
    MeteringFinderNL(VarMan &varMan, const GuardList &guard, const std::vector<UpdateMap> &update);

    /**
     * Simplifies guard/update by removing constraints that do not affect the metering function.
     * While doing so, computes reducedGuard, irrelevantGuard and relevantVars.
     */
    void simplifyAndFindVariables();

    /**
     * Performs all available pre-processing steps:
     *  - eliminates temporary variables (where possible)
     *  - simplifies guard/update (calls simplifyAndFindVariables)
     *  - linearizes guard/update (if possible)
     *
     * Sets/modifies the members: guard, update, reducedGuard, irrelevantGuard, relevantVars.
     *
     * @return true iff linearization was successful
     */
    bool preprocessAndLinearize();

    /**
     * Uses relevantVars to set meterVars (symbols and coefficients).
     * The type of the generated coefficients depends on FARKAS_ALLOW_REAL_COEFFS.
     */
    void buildMeteringVariables();

    /**
     * Builds the required lists of constraints (guard, reducedGuard, guardUpdate)
     * in the form "linear term <= constant"
     */
    void buildLinearConstraints();

    /**
     * Helper to build the implication: "(G and U) --> f(x)-f(x') <= 1" using applyFarkas
     */
    z3::expr genUpdateImplications() const;

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
     * Helper to build constraints to suppress trivial solutions, i.e. "OR c_i != 0" for the coefficients c_i
     */
    z3::expr genNonTrivial() const;

    /**
     * Given the z3 model, builds the corresponding linear metering function and applies the reverse substitution nonlinearSubs
     */
    Expression buildResult(const z3::model &model) const;

    /**
     * Tries to find a pair conflicting variables A, B.
     * We call two variables conflicting if we would need min(A,B) or max(A,B) in the metering function
     * (which we can currently not express). Example: A++, B++ [ A < X, B < Y ].
     * Note that this is just a heuristic that only handles simple cases.
     */
    boost::optional<VariablePair> findConflictVars() const;

    /**
     * Modifies the current result to ensure that the metering function evaluates to an integer.
     * To this end, replaces the metering function with a fresh variable and adds a constraint
     * (to be added to the rule's guard) that relates the fresh variable with the original metering function.
     *
     * Note: Only required if FARKAS_ALLOW_REAL_COEFFS is set.
     */
    void ensureIntegralMetering(Result &result, const z3::model &model) const;


    void dump(const std::string &msg) const;


private:
    /**
     * The VariableManager instance, used for fresh variables and for conversion between indices/symbols
     */
    VariableManager &varMan;

    /**
     * The Z3 context to handle z3 symbols/expressions
     */
    mutable Z3Context context;



    /**
     * The rule's data, is modified by linearization and when restricting to relevant variables
     */
    std::vector<UpdateMap> updates;
    GuardList guard;

    /**
     * Same as guard, but only contains constraints that (might) limit the execution of the loop.
     * irrelevantGuard is the guard without the reducedGuard (so the constraints that were dropped).
     */
    GuardList reducedGuard;
    GuardList irrelevantGuard;

    /**
     * The set of variables that might occur in the metering function.
     * These variables are used to build the template for the metering function.
     */
    std::set<VariableIdx> relevantVars;

    /**
     * Reverse substitution from linearization
     */
    GiNaC::exmap nonlinearSubs;



    /**
     * Linear constraints (of the form "linear term <= constant")
     * obtained from guard, reduced guard, irrelevant guard, guard and update.
     */
    struct {
        GuardList guard;
        GuardList reducedGuard;
        GuardList irrelevantGuard;
        std::vector<GuardList> guardUpdate; // one for each update
    } linearConstraints;

    /**
     * Variables and their coefficients for the metering function template.
     * primedSymbols maps updated variables to a primed version of the variable's symbol.
     */
    struct {
        std::vector<ExprSymbol> symbols;
        std::vector<z3::expr> coeffs;
        std::map<VariableIdx,ExprSymbol> primedSymbols;
    } meterVars;

    /**
     * The absolute coefficient for the metering function template.
     */
    z3::expr absCoeff;

};

#endif // METERING_H
