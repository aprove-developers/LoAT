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

#include "nl_chaining.h"

#include "preprocess.h"
#include "z3/z3toolbox.h"

#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"



using namespace std;
using boost::optional;



// #####################
// ##  Chaining Core  ##
// #####################


/**
 * Helper for chainRules. Checks if the given (chained) guard is satisfiable.
 * If z3 returns unknown, applies some heuristic (which involves the (chained) cost).
 */
static bool checkSatisfiable(const GuardList &newGuard, const Expression &newCost) {
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


optional<NonlinearRule> ChainingNL::chainRulesOnRhs(const VarMan &varMan,
                                                    const NonlinearRule &first, int firstRhsIdx,
                                                    const NonlinearRule &second)
{
    // Build a substitution corresponding to the first rule's update
    GiNaC::exmap firstUpdate = first.getUpdate(firstRhsIdx).toSubstitution(varMan);

    // Concatenate both guards, but apply the first rule's update to second guard
    GuardList newGuard = first.getGuard();
    for (const Expression &ex : second.getGuard()) {
        newGuard.push_back(ex.subs(firstUpdate));
    }

    // Add the costs, but apply first rule's update to second cost
    Expression newCost = first.getCost() + second.getCost().subs(firstUpdate);

    // As a small optimization: Keep an INF symbol (easier to identify INF cost later on)
    if (first.getCost().isInfty() || second.getCost().isInfty()) {
        newCost = Expression::InfSymbol;
    }

#ifdef CONTRACT_CHECK_SAT

    // FIXME: If z3 says unsat, it makes no sense to keep both transitions (and try to chain them again and again later on)
    // FIXME: Instead, the second one should be deleted, at least in chainLinearPaths (where we can be sure that the second transition is only reachable by the first one).

    // FIXME: We have to think about the z3::unknown case. Since z3 often takes long on unknowns, calling z3 again and again is pretty bad.
    // FIXME: So either we remove the second transition (might lose complexity if the guard was actually sat) or chain them (lose complexity if it was unsat) => BENCHMARKS

    // Avoid chaining if the resulting rule can never be taken
    if (!checkSatisfiable(newGuard, newCost)) {
        Stats::add(Stats::ContractUnsat);
        debugChain("Cannot chain rules due to z3::unsat/unknown: " << first << " + " << second);
        return {};
    }
#endif

    vector<RuleRhs> newRhss;
    const vector<RuleRhs> &firstRhss = first.getRhss();

    // keep the first rhss of first
    for (int i=0; i < firstRhsIdx; ++i) {
        newRhss.push_back(firstRhss[i]);
    }

    // insert the rhss of second, composed with first's update
    for (const RuleRhs &secondRhs : second.getRhss()) {
        UpdateMap newUpdate = first.getUpdate(firstRhsIdx);
        for (const auto &it : secondRhs.getUpdate()) {
            newUpdate[it.first] = it.second.subs(firstUpdate);
        }
        newRhss.push_back(RuleRhs(secondRhs.getLoc(), newUpdate));
    }

    // keep the last rhss of first
    for (int i=firstRhsIdx+1; i < firstRhss.size(); ++i) {
        newRhss.push_back(firstRhss[i]);
    }

    return NonlinearRule(RuleLhs(first.getLhsLoc(), newGuard, newCost), newRhss);
}


optional<NonlinearRule> ChainingNL::chainRules(const VarMan &varMan, const NonlinearRule &first, const NonlinearRule &second) {
    NonlinearRule res = first;

    // Iterate over rhss, note that the number of rhss can increase while iterating
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
            // (this is important in the case that second is a selfloop)
            rhsIdx += second.rhsCount();
        } else {
            rhsIdx += 1;
        }
    }

    return res;
}



// ##############################
// ##  Helpers for Strategies  ##
// ##############################


/**
 * Eliminates the given location by chaining every incoming with every outgoing transition.
 * Note: The given location must not have any self-loops (simple or non-simple).
 *
 * If keepUnchainable is true and some incoming transition T cannot be chained with at least one outgoing transition,
 * then a new dummy location is inserted and T is kept, connecting its its old source to the new dummy location.
 * However, this is only done if the cost of T is more than constant.
 *
 * The old location is removed, together with all old transitions. So if an outgoing transition cannot be chained
 * with any incoming transition, it will simply be removed.
 *
 * @note: This will also chain self-loops with itself.
 */
static void eliminateLocationByChaining(ITSProblem &its, LocationIdx loc, bool keepUnchainable) {
    set<TransIdx> keepRules;
    debugChain("  eliminating location " << loc << " by chaining (keep unchainable: " << keepUnchainable << ")");

    // Chain all pairs of in- and outgoing rules
    for (TransIdx in : its.getTransitionsTo(loc)) {
        bool wasChained = false;
        const NonlinearRule &inRule = its.getRule(in);

        // If a loop starts in loc, it (and all chained versions of it) will be removed anyway, so we skip it.
        // Note that this only happens for rules with self-loops, e.g. f -> f,g (where f is loc)
        if (inRule.getLhsLoc() == loc) continue;

        for (TransIdx out : its.getTransitionsFrom(loc)) {
            auto optRule = ChainingNL::chainRules(its, inRule, its.getRule(out));
            if (optRule) {
                wasChained = true;
                TransIdx added = its.addRule(optRule.get());
                debugChain("    chained " << in << " and " << out << " to new rule: " << added);
            } else {
                debugChain("    failed to chain " << in << " and " << out);
            }
        }

        if (keepUnchainable && !wasChained) {
            // Only keep the rule if it might give non-trivial complexity
            if (inRule.getCost().getComplexity() > Complexity::Const) {
                keepRules.insert(in);
            }
        }
    }

    // Backup all incoming transitions which could not be chained with any outgoing one
    if (keepUnchainable && !keepRules.empty()) {
        LocationIdx dummyLoc = its.addLocation();
        for (TransIdx trans : keepRules) {
            // keep lhs, ignore rhss and updates
            NonlinearRule backup(its.getRule(trans).getLhs(), RuleRhs(dummyLoc, {}));
            TransIdx added = its.addRule(backup);
            debugChain("    keeping rule " << trans << " by adding dummy rule: " << added);
        }
    }

    // Remove all outgoing rules from loc
    for (TransIdx idx : its.getTransitionsFrom(loc)) {
        its.removeRule(idx);
    }

    // In case of nonlinear rules, we do not want to remove all rules leading to loc.
    // Instead, we only remove rules where _all_ rhss lead to loc, but keep the other rules.
    // If all rules are linear, this is equivalent to just removing all rules.
    auto rhsLeadsToLoc = [&](const RuleRhs &rhs){ return rhs.getLoc() == loc; };

    for (TransIdx idx : its.getTransitionsTo(loc)) {
        const NonlinearRule &rule = its.getRule(idx);
        if (std::all_of(rule.rhsBegin(), rule.rhsEnd(), rhsLeadsToLoc)) {
            its.removeRule(idx);
        }
    }

    // Remove the location if it is no longer used
    if (!its.hasTransitionsTo(loc)) {
        its.removeOnlyLocation(loc);
    }
}


/**
 * Implementation of dfsTraversalWithRepeatedChanges
 */
template <typename F>
static bool callRepeatedlyImpl(ITSProblem &its, F function, LocationIdx node, set<LocationIdx> &visited) {
    if (!visited.insert(node).second) {
        return false;
    }

    bool changedOverall = false;
    bool changed;

    // Call the function repeatedly, until it returns false
    do {
        changed = function(its, node);
        changedOverall = changedOverall || changed;

        if (Timeout::soft()) {
            return changedOverall;
        }

    } while (changed);

    // Continue with the successors of the current node (DFS traversal)
    for (LocationIdx next : its.getSuccessorLocations(node)) {
        bool changed = callRepeatedlyImpl(its, function, next, visited);
        changedOverall = changedOverall || changed;

        if (Timeout::soft()) {
            return changedOverall;
        }
    }

    return changedOverall;
}


/**
 * A dfs traversal through the its's graph, starting in the initial location, calling function for each node.
 *
 * The given function must return a boolean value (a "changed" flag). It is called several times on every
 * visited node, as long as it returns true. If it returns false, the DFS continues with the next node.
 * The function is allowed to modify the ITS (and thus the graph).
 *
 * The given set `visited` should be empty.
 *
 * The return value is true iff at least one call of the given function returned true.
 */
template <typename F>
static bool callRepeatedlyOnEachNode(ITSProblem &its, F function) {
    set<LocationIdx> visited;
    return callRepeatedlyImpl(its, function, its.getInitialLocation(), visited);
}


/**
 * Checks whether the given node lies on a linear path (and is not an endpoint of the path).
 * See chainLinearPaths for an explanation.
 */
static bool isOnLinearPath(ITSProblem &its, LocationIdx node) {
    // If node is a leaf, we return false (we cannot chain over leafs)
    if (its.getTransitionsFrom(node).size() != 1) {
        return false;
    }

    // The node must not have two (or zero) predecessors ...
    set<LocationIdx> preds = its.getPredecessorLocations(node);
    if (preds.size() != 1) {
        return false;
    }

    // ... it must not have a self-loop ...
    if (preds.count(node) > 0) {
        return false;
    }

    // ... or two (or zero) incoming edges (possibly from the same predecessor)
    LocationIdx pred = *preds.begin();
    return its.getTransitionsFromTo(pred, node).size() == 1;
}



// ############################
// ##  Chaining  Strategies  ##
// ############################


bool ChainingNL::chainLinearPaths(ITSProblem &its) {
    auto implementation = [](ITSProblem &its, LocationIdx node) {
        bool changed = false;
        for (LocationIdx succ : its.getSuccessorLocations(node)) {

            // Avoid chaining over the initial node (it would then be removed) and skip self-loops
            if (its.isInitialLocation(succ) || succ == node) {
                continue;
            }

            // Only apply chaining if succ has exactly one in- and one outgoing transition
            if (isOnLinearPath(its, succ)) {
                eliminateLocationByChaining(its, succ, true);
                changed = true;
                Stats::add(Stats::ContractLinear);
            }
        }
        return changed;
    };

    Timing::Scope timer(Timing::Contract);
    debugChain("Chaining linear paths");
    return callRepeatedlyOnEachNode(its, implementation);
}


bool ChainingNL::chainTreePaths(ITSProblem &its) {
    auto implementation = [](ITSProblem &its, LocationIdx node) {
        bool changed = false;
        for (LocationIdx succ : its.getSuccessorLocations(node)) {

            // Avoid chaining over the initial node (it would then be removed) and skip self-loops
            if (its.isInitialLocation(succ) || succ == node) {
                continue;
            }

            // if succ has several predecessors, try contracting the rest first (succ might be a loop head)
            if (its.getPredecessorLocations(succ).size() > 1) {
                continue;
            }

            // Chain transitions from node to succ with all transitions from succ.
            if (!its.getSuccessorLocations(succ).empty()) {
                eliminateLocationByChaining(its, succ, true);
                changed = true;
            }

            Stats::add(Stats::ContractBranch);
            if (Timeout::soft()) break;
        }
        return changed;
    };

    Timing::Scope timer(Timing::Branches);
    debugChain("Chaining tree paths");
    return callRepeatedlyOnEachNode(its, implementation);

    // TODO: Restore this functionality, i.h., call removeConstLeafsAndUnreachable after calling chainTreePaths (wherever it is called)
    // TODO: Since we now use eliminateLocationByChaining, this is probably not needed
    // TODO: (but of course might still remove nodes that were unreachable even before calling this, so we have to check whether it makes a difference!)
    //only for the main caller, reduce unreachable stuff
    // if (node == initial) {
    //     removeConstLeafsAndUnreachable();
    // }
}

/**
 * Implementation of eliminateALocation
 */
static bool eliminateALocationImpl(ITSProblem &its, LocationIdx node, set<LocationIdx> &visited) {
    if (!visited.insert(node).second) {
        return false;
    }

    debugChain("  checking if we can eliminate location " << node);

    bool hasIncoming = its.hasTransitionsTo(node);
    bool hasOutgoing = its.hasTransitionsFrom(node);
    bool hasSimpleLoop = !its.getSimpleLoopsAt(node).empty();

    // If we cannot eliminate node, continue with its children (DFS traversal)
    if (hasSimpleLoop || its.isInitialLocation(node) || !hasIncoming || !hasOutgoing) {
        for (LocationIdx succ : its.getSuccessorLocations(node)) {
            if (eliminateALocationImpl(its, succ, visited)) {
                return true;
            }

            if (Timeout::soft()) {
                return false;
            }
        }
        return false;
    }

    // Otherwise, we can eliminate node
    debugChain("  found location to eliminate: " << node);
    eliminateLocationByChaining(its, node, true);
    return true;
}


bool ChainingNL::eliminateALocation(ITSProblem &its) {
    Timing::Scope timer(Timing::Contract);
    Stats::addStep("ChainingNL::eliminateALocation");
    debugChain("Trying to eliminate a location");

    set<NodeIndex> visited;
    return eliminateALocationImpl(its, its.getInitialLocation(), visited);
}


bool ChainingNL::chainAcceleratedLoops(ITSProblem &its, const std::set<TransIdx> &acceleratedLoops) {
    Timing::Scope timer(Timing::Contract);
    Stats::addStep("ChainingNL::chainSimpleLoops");

    for (TransIdx accel : acceleratedLoops) {
        debugChain("Chaining accelerated rule " << accel);
        const NonlinearRule &accelRule = its.getRule(accel);
        LocationIdx node = accelRule.getLhsLoc();

        for (TransIdx incoming : its.getTransitionsTo(node)) {
            const NonlinearRule &incomingRule = its.getRule(incoming);

            // Do not chain with incoming loops that are themselves self-loops at node
            // (no matter if they are simple or not)
            if (incomingRule.getLhsLoc() == node) continue;

            // Do not chain with another accelerated rule (already be covered by the previous check)
            assert(acceleratedLoops.count(incoming) == 0);

            auto optRule = ChainingNL::chainRules(its, incomingRule, accelRule);
            if (optRule) {
                TransIdx added = its.addRule(optRule.get());
                debugChain("  chained incoming rule " << incoming << " with " << accel << ", resulting in new rule: " << added);
            }
        }

        debugChain("  removing accelerated rule " << accel);
        its.removeRule(accel);
    }

    return !acceleratedLoops.empty();
}
