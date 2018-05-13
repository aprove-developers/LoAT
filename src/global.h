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

#ifndef GLOBAL_H
#define GLOBAL_H

/*
 * if defined, proof output is enabled (might be useful to disable during debugging)
 */
#define PROOF_OUTPUT_ENABLE

/*
 * these defines control whether the output is colored using ANSI color codes
 * (disable all defines if your terminal does not support ANSI escape sequences)
 */
#define COLORS_PROOF
#define COLORS_DEBUG
#define COLORS_ITS_EXPORT

// enable debugging and access to proof output (after the defines for COLORS)
#include "debug.h"
#include "util/proofoutput.h"

/*
 * if defined, it is checked if the combined guard is SAT before each chaining step
 * NOTE: as z3 cannot handle non-linear integer arithmetic well, there are a lot of false positives!
 */
#define CONTRACT_CHECK_SAT

/*
 * if defined, in case of unknown for the SAT check an approximate (not 100% sound) check is done
 * (currently, this is done by treating all variables/constants as reals instead of integers)
 */
#define CONTRACT_CHECK_SAT_APPROXIMATE

/*
 * if defined, in case of unknown for the SAT check edges are still chained if
 * the resulting cost is exponential (z3 cannot handle exponentials)
 */
#define CONTRACT_CHECK_EXP_OVER_UNKNOWN

/*
 * if defined, the number of transitions is heuristically recuded (see below)
 * after every branch contraction step (which usually creates many new transitions)
 */
#define PRUNING_ENABLE

/*
 * Allow not more than this amount of parallel transitions (i.e. between the same two nodes)
 * If there are more, we apply pruning and remove the ones with the lowest cost [GREEDY!]
 * (note that we currently use the Infinity check here, to not be mistaken by high costs, which are effectively constant)
 */
#define PRUNE_MAX_PARALLEL_TRANSITIONS 5

/*
 * if defined, a preprocessing of the transition will always be performed before trying to meter a selfloop
 * NOTE: as the preprocessing is meant to be run rarely, this might have a performance impact
 * NOTE: currently, the farkas code does some simple preprocessing on its own anyway!
 */
#define SELFLOOPS_ALWAYS_SIMPLIFY

/*
 * this defines the nesting iterations when eliminating selfloops, i.e.
 * how often the successfully nested loops are tried to be nested again
 * NOTE: nesting is always aborted if no new nested loops are created
 */
#define NESTING_MAX_ITERATIONS 3
// FIXME: The value 3 is probably too high, 2 should really be sufficient, check on benchmarks


// TODO: This heuristic has been removed
/*
 * if defined, ranked selfloops are chained (if possible).
 * This increases time and branches, but might be valuable in certain examples.
 * (e.g. if you first want to take one path, then the second one through a single loop body)
 */
//#define NESTING_CHAIN_RANKED


/*
 * If defined, all simple loops of one location are chained with each other
 * before trying to accelerate them. This increases time and the number of rules,
 * but might help in certain examples (e.g. if we have a loop body with two different
 * paths A, B which we want to alternate, i.e., A, B, A, B, A, B, ...)
 */
//#define CHAIN_BEFORE_ACCELERATE
// TODO: CHAIN_AFTER_ACCELERATE


/*
 * if defined, in case of an unsat result from farkas, some additional constraints are added to the guard
 * (similar to chaining, but without modifying the update).
 * NOTE: this might help for some loops (with constant update rhs), but also increases time and branching
 */
// Note: This was disabled in the old version!
//#define FARKAS_TRY_ADDITIONAL_GUARD

/*
 * if defined, real coefficients are allowed in metering functions.
 * (this requires extending the transition's guard to ensure the result is always an integer)
 */
#define FARKAS_ALLOW_REAL_COEFFS

/*
 * if defined, a simple heuristic is used that allows adding A > B and (for a copy of the transition) B > A
 * to the guard in cases where the metering function would likely be of the form min(A,B) or max(A,B).
 * I.e. the loop's termination depends on two variables, and we do not know which limit is hit first,
 * thus add A > B rsp. B > A to make the problem solvable. Note that the heuristic is quite simple.
 */
#define FARKAS_HEURISTIC_FOR_MINMAX

/*
 * the maximum number of bounds that are tried for a single free variable,
 * when instantiation is applied in farkas code (this limit is there to prevent exponential complexity)
 */
#define FARKAS_HEURISTIC_INSTANTIATE_FREEVARS
#define FREEVAR_INSTANTIATE_MAXBOUNDS 3

/*
 * the max exponent n up to which a power of the form expr^n is rewritten as multiplication.
 * (z3 does not support exponents well, while multiplication works with bit-blasting)
 * NOTE: increasing this might make z3 run extremely long on certain examples
 */
#define Z3_MAX_EXPONENT 5

/*
 * timeouts for z3 in ms
 */
#define Z3_CHECK_TIMEOUT 100u
#define Z3_LIMITSMT_TIMEOUT 500u

/*
 * if defined, the final guard/cost is checked to ensure it has infintily many instances
 * NOTE: this check is strongly required for soundness (should never be disabled anymore)
 */
#define FINAL_INFINITY_CHECK

/*
 * discard a limit problem of size >= LIMIT_PROBLEM_DISCARD_SIZE in a non-final check
 * if z3 yields "unknown"
 */
#define LIMIT_PROBLEM_DISCARD_SIZE 10


// settings (can be specified on the command line)
namespace GlobalFlags {
    extern bool limitSmt;
}

// global variables
namespace GlobalVariables {
    extern ProofOutput proofOutput;
}

// shorthands for global variables
#define proofout (GlobalVariables::proofOutput)

// dummy stream (to disable proof output)
#ifndef PROOF_OUTPUT_ENABLE
struct DummyStream {
    static std::ostream dummy;
};
#endif

#endif //GLOBAL_H
