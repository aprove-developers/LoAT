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

#include "chain.h"

#include "z3/z3toolbox.h"
#include "util/stats.h"
#include "debug.h"

using namespace std;


// ############################
// ##  Satisfiability Check  ##
// ############################

/**
 * Helper for chainRules. Checks if the given (chained) guard is satisfiable.
 * If z3 returns unknown, applies some heuristic (which involves the (chained) cost).
 */
static bool checkSatisfiability(const GuardList &newGuard, const Expression &newCost) {
    auto z3res = Z3Toolbox::checkAll(newGuard);

    // FIXME: Only ask z3 for polynomials, since z3 cannot handle exponentials well
    // FIXME: Just keep exponentials (hopefully there are not that many)
    // FIXME: This is probably better than using CONTRACT_CHECK_EXP_OVER_UNKNOWN

#ifdef CONTRACT_CHECK_SAT_APPROXIMATE
    // TODO: Might be a good idea to remove non-polynomial guards here, or better in checkAllApproximate?!
    // Try to solve an approximate problem instead, as we the check does not affect soundness.
    if (z3res == z3::unknown) {
        // TODO: use operator<< for newGuard when implemented
        debugProblem("Contract unknown, try approximation for guard: ");
        dumpList("guard", newGuard);
        z3res = Z3Toolbox::checkAllApproximate(newGuard);
    }
#endif

#ifdef CONTRACT_CHECK_EXP_OVER_UNKNOWN
    // Treat unknown as sat if the new cost is exponential
    if (z3res == z3::unknown && newCost.getComplexity() == Complexity::Exp) {
        debugChain("Ignoring z3::unknown because of exponential cost");
        return true;
    }
#endif

#ifdef DEBUG_PROBLEMS
    if (z3res == z3::unknown) {
        // TODO: use operator<< for newGuard when implemented
        debugProblem("Chaining: got z3::unknown for: ");
        dumpList("guard", newGuard);
    }
#endif

    return z3res == z3::sat;
}


// ########################
// ##  Chaining Helpers  ##
// ########################

/**
 * Part of the main chaining algorithm.
 * Chains the given first rule's lhs with the second rule's lhs,
 * by applying the first rule's update to the second rule's lhs (guard/cost).
 * Also checks whether the resulting guard is satisfiable (and returns none if not).
 */
static option<RuleLhs> chainLhss(const VarMan &varMan, const RuleLhs &firstLhs, const UpdateMap &firstUpdate,
                                 const RuleLhs &secondLhs)
{
    // Build a substitution corresponding to the first rule's update
    GiNaC::exmap updateSubs = firstUpdate.toSubstitution(varMan);

    // Concatenate both guards, but apply the first rule's update to second guard
    GuardList newGuard = firstLhs.getGuard();
    for (const Expression &ex : secondLhs.getGuard()) {
        newGuard.push_back(ex.subs(updateSubs));
    }

    // Add the costs, but apply first rule's update to second cost
    Expression newCost = firstLhs.getCost() + secondLhs.getCost().subs(updateSubs);

    // As a small optimization: Keep an INF symbol (easier to identify INF cost later on)
    if (firstLhs.getCost().isInfSymbol() || secondLhs.getCost().isInfSymbol()) {
        newCost = Expression::InfSymbol;
    }

#ifdef CONTRACT_CHECK_SAT

    // FIXME: If z3 says unsat, it makes no sense to keep both transitions (and try to chain them again and again later on)
    // FIXME: Instead, the second one should be deleted, at least in chainLinearPaths (where we can be sure that the second transition is only reachable by the first one).

    // FIXME: We have to think about the z3::unknown case. Since z3 often takes long on unknowns, calling z3 again and again is pretty bad.
    // FIXME: So either we remove the second transition (might lose complexity if the guard was actually sat) or chain them (lose complexity if it was unsat) => BENCHMARKS

    // Avoid chaining if the resulting rule can never be taken
    if (!checkSatisfiability(newGuard, newCost)) {
        Stats::add(Stats::ContractUnsat);
        return {};
    }
#endif

    return RuleLhs(firstLhs.getLoc(), newGuard, newCost);
}


/**
 * Part of the main chaining algorithm.
 * Composes the two given updates (such that firstUpdate is applied before secondUpdate)
 */
static UpdateMap chainUpdates(const VarMan &varMan, const UpdateMap &first, const UpdateMap &second) {
    // Start with the first update
    UpdateMap newUpdate = first;
    const GiNaC::exmap firstSubs = first.toSubstitution(varMan);

    // Then add the second update (possibly overwriting the first updates).
    // Note that we apply the first update to the second update's right-hand sides.
    for (const auto &it : second) {
        newUpdate[it.first] = it.second.subs(firstSubs);
    }
    return newUpdate;
}


// #######################
// ##  Linear Chaining  ##
// #######################

/**
 * Special case for chaining linear rules.
 * The behaviour is the same as for general rules, but the implementation is simpler (and possibly faster).
 */
static option<LinearRule> chainLinearRules(const VarMan &varMan, const LinearRule &first, const LinearRule &second) {
    assert(first.getRhsLoc() == second.getLhsLoc());

    auto newLhs = chainLhss(varMan, first.getLhs(), first.getUpdate(), second.getLhs());
    if (!newLhs) {
        debugChain("Cannot chain rules due to z3::unsat/unknown: " << first << " + " << second);
        return {};
    }

    UpdateMap newUpdate = chainUpdates(varMan, first.getUpdate(), second.getUpdate());
    return LinearRule(newLhs.get(), RuleRhs(second.getRhsLoc(), newUpdate));
}


// ##########################
// ##  Nonlinear Chaining  ##
// ##########################


// FIXME: Rename free variables!
/**
 * Chains the specified right-hand side of the first rule (specified by firstRhsIdx)
 * with the second rule (the locations must match).
 * @return The resulting rule, unless it can be shown to be unsatisfiable.
 */
static option<Rule> chainRulesOnRhs(const VarMan &varMan, const Rule &first, int firstRhsIdx, const Rule &second) {
    const UpdateMap &firstUpdate = first.getUpdate(firstRhsIdx);

    auto newLhs = chainLhss(varMan, first.getLhs(), firstUpdate, second.getLhs());
    if (!newLhs) {
        debugChain("Cannot chain rules due to z3::unsat/unknown: " << first << " + " << second);
        return {};
    }

    vector<RuleRhs> newRhss;
    const vector<RuleRhs> &firstRhss = first.getRhss();

    // keep the first rhss of first (up to the one we want to chain)
    for (int i=0; i < firstRhsIdx; ++i) {
        newRhss.push_back(firstRhss[i]);
    }

    // insert the rhss of second, chained with first's update
    for (const RuleRhs &secondRhs : second.getRhss()) {
        UpdateMap newUpdate = chainUpdates(varMan, firstUpdate, secondRhs.getUpdate());
        newRhss.push_back(RuleRhs(secondRhs.getLoc(), newUpdate));
    }

    // keep the last rhss of first (after the one we want to chain)
    for (int i=firstRhsIdx+1; i < firstRhss.size(); ++i) {
        newRhss.push_back(firstRhss[i]);
    }

    return Rule(newLhs.get(), newRhss);
}


/**
 * Implementation of chaining for nonlinear rules,
 * chains all rhss that lead to second's lhs loc with second.
 */
static option<Rule> chainNonlinearRules(const VarMan &varMan, const Rule &first, const Rule &second) {
    Rule res = first;

    // Iterate over rhss, chain every rhs whose location matches second's lhs location.
    // Note that the number of rhss can increase while iterating (due to chaining).
    // The order of the rhss is preserved, a single rhs is replaced by all rhss resulting from chaining.
    int rhsIdx = 0;
    while (rhsIdx < res.rhsCount()) {
        if (first.getRhsLoc(rhsIdx) == second.getLhsLoc()) {
            auto chained = chainRulesOnRhs(varMan, res, rhsIdx, second);
            if (!chained) {
                // we have to chain all rhss that lead to the second rule,
                // so we give up if any of the chaining operations fails.
                return {};
            }

            // update res to the result from chaining
            res = chained.get();

            // skip the rhss that were inserted from the second rule
            // (this is important in the case that second has a selfloop)
            rhsIdx += second.rhsCount();
        } else {
            rhsIdx += 1;
        }
    }

    return res;
}


// ########################
// ##  Public Interface  ##
// ########################

option<Rule> Chaining::chainRules(const VarMan &varMan, const Rule &first, const Rule &second) {
    // Use the simpler/faster implementation if applicable (even if we have to copy for the conversion)
    if (first.isLinear() && second.isLinear()) {
        auto res = chainLinearRules(varMan, first.toLinear(), second.toLinear());
        if (res) {
            return res.get(); // implicit cast from LinearRule to Rule
        }
        return {};
    }

    return chainNonlinearRules(varMan, first, second);
}

option<LinearRule> Chaining::chainRules(const VarMan &varMan, const LinearRule &first, const LinearRule &second) {
    return chainLinearRules(varMan, first.toLinear(), second.toLinear());
}
