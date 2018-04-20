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

#include "chaining.h"

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
    // Try to solve an approximate problem instead, as we the check does not affect soundness.
    if (z3res == z3::unknown) {
        debugProblem("Contract unknown, try approximation for guard: " << newGuard);
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
        debugProblem("Chaining: got z3::unknown for: " << newGuard);
    }
#endif

    return z3res == z3::sat;
}


optional<LinearRule> Chaining::chainRules(const VarMan &varMan, const LinearRule &first, const LinearRule &second) {
    // Build a substitution corresponding to the first rule's update
    GiNaC::exmap updateSubs;
    for (auto it : first.getUpdate()) {
        updateSubs[varMan.getGinacSymbol(it.first)] = it.second;
    }

    // Concatenate both guards, but apply the first rule's update to second guard
    GuardList newGuard = first.getGuard();
    for (const Expression &ex : second.getGuard()) {
        newGuard.push_back(ex.subs(updateSubs));
    }

    // Add the costs, but apply first rule's update to second cost
    Expression newCost = first.getCost() + second.getCost().subs(updateSubs);

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
        debugChain("Aborting due to z3::unsat/unknown for rules: " << first << " + " << second);
        return {};
    }
#endif

    // Compose both updates
    UpdateMap newUpdate;
    for (auto it : second.getUpdate()) {
        newUpdate[it.first] = it.second.subs(updateSubs);
    }

    return LinearRule(first.getLhsLoc(), newGuard, newCost, second.getRhsLoc(), newUpdate);
}



// ##############################
// ##  Helpers for Strategies  ##
// ##############################


/**
 * Eliminates the given location by chaining every incoming with every outgoing transition.
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
static void eliminateLocationByChaining(LinearITSProblem &its, LocationIdx loc, bool keepUnchainable) {
    set<LinearRule> keepRules;

    // Chain all pairs of in- and outgoing rules
    for (TransIdx in : its.getTransitionsTo(loc)) {
        bool wasChained = false;
        const LinearRule &inRule = its.getRule(in);

        for (TransIdx out : its.getTransitionsFrom(loc)) {
            auto optRule = Chaining::chainRules(its, inRule, its.getRule(out));
            if (optRule) {
                wasChained = true;
                its.addRule(optRule.get());
            }
        }

        if (keepUnchainable && !wasChained) {
            // Only keep the rule if it might give non-trivial complexity
            if (inRule.getCost().getComplexity() > Complexity::Const) {
                keepRules.insert(inRule);
            }
        }
    }

    // remove the location and all incoming/outgoing transitions
    its.removeLocationAndRules(loc);

    // re-add all incoming transitions which could not be chained with any outgoing one
    if (keepUnchainable && !keepRules.empty()) {
        LocationIdx dummyLoc = its.addLocation();
        for (const LinearRule &rule : keepRules) {
            its.addRule(rule.withNewRhsLoc(dummyLoc));
        }
    }
}


/**
 * Implementation of dfsTraversalWithRepeatedChanges
 */
template <typename F>
static bool callRepeatedlyImpl(LinearITSProblem &its, F function, LocationIdx node, set<LocationIdx> &visited) {
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
static bool callRepeatedlyOnEachNode(LinearITSProblem &its, F function) {
    set<LocationIdx> visited;
    return callRepeatedlyImpl(its, function, its.getInitialLocation(), visited);
}


/**
 * Checks whether the given node lies on a linear path (and is not an endpoint of the path).
 * See chainLinearPaths for an explanation.
 */
static bool isOnLinearPath(LinearITSProblem &its, LocationIdx node) {
    // If node is a leaf, we return false (we cannot chain over leafs)
    if (its.getTransitionsFrom(node).size() != 1) {
        return false;
    }

    // The node must not have two (or zero) predecessors ...
    set<LocationIdx> preds = its.getPredecessorLocations(node);
    if (preds.size() != 1) {
        return false;
    }

    // ... or two (or zero) incoming edges (possibly from the same predecessor)
    LocationIdx pred = *preds.begin();
    return its.getTransitionsFromTo(pred, node).size() == 1;
}



// ############################
// ##  Chaining  Strategies  ##
// ############################


bool Chaining::chainLinearPaths(LinearITSProblem &its) {
    auto implementation = [](LinearITSProblem &its, LocationIdx node) {
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
    Stats::addStep("Chaining::chainLinear");
    return callRepeatedlyOnEachNode(its, implementation);
}


bool Chaining::chainTreePaths(LinearITSProblem &its) {
    auto implementation = [](LinearITSProblem &its, LocationIdx node) {
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
            eliminateLocationByChaining(its, succ, true);

            changed = true;
            Stats::add(Stats::ContractBranch);
            if (Timeout::soft()) break;
        }
        return changed;
    };

    Timing::Scope timer(Timing::Branches);
    Stats::addStep("Chaining::chainBranches");
    return callRepeatedlyOnEachNode(its, implementation);

    // TODO: Restore this functionality, i.h., call removeConstLeafsAndUnreachable after calling chainTreePaths (wherever it is called)
    //only for the main caller, reduce unreachable stuff
    // if (node == initial) {
    //     removeConstLeafsAndUnreachable();
    // }
}


/**
 * Implementation of eliminateALocation
 */
static bool eliminateALocationImpl(LinearITSProblem &its, LocationIdx node, set<LocationIdx> &visited) {
    if (!visited.insert(node).second) {
        return false;
    }

    debugChain("trying to eliminate location " << node);

    vector<TransIdx> transIn = its.getTransitionsTo(node);
    vector<TransIdx> transOut = its.getTransitionsFrom(node);

    bool hasSelfloop = its.getTransitionsFromTo(node, node).empty();

    // If we cannot eliminate node, continue with its children (DFS traversal)
    if (hasSelfloop || its.isInitialLocation(node) || transIn.empty() || transOut.empty()) {
        for (LocationIdx succ : its.getSuccessorLocations(node)) {
            if (eliminateALocation(its, succ, visited)) {
                return true;
            }

            if (Timeout::soft()) {
                return false;
            }
        }
        return false;
    }

    // Otherwise, we can eliminate node
    eliminateLocationByChaining(its, node, true);
    return true;
}


bool Chaining::eliminateALocation(LinearITSProblem &its) {
    Timing::Scope timer(Timing::Contract);
    Stats::addStep("Chaining::eliminateALocation");

    set<NodeIndex> visited;
    return eliminateALocationImpl(its, its.getInitialLocation(), visited);
}


/**
 * Core implementation for chainSimpleLoops
 */
static bool chainSimpleLoopsAt(LinearITSProblem &its, LocationIdx node) {
    debugChain("Chaining simple loops.");
    assert(!its.isInitialLocation(node));
    assert(!its.getTransitionsFromTo(node, node).empty());

    set<TransIdx> successfullyChained;
    vector<TransIdx> transIn = its.getTransitionsTo(node);

    for (TransIdx simpleLoop : its.getTransitionsFromTo(node, node)) {
        for (TransIdx incoming : transIn) {
            const LinearRule &incomingRule = its.getRule(incoming);

            // Skip simple loops
            if (incomingRule.getLhsLoc() == incomingRule.getRhsLoc()) {
                continue;
            }

            auto optRule = chainRules(its, incomingRule, its.getRule(simpleLoop));
            if (optRule) {
                its.addRule(optRule.get());
                successfullyChained.insert(incoming);
            }
        }

        debugChain("removing simple loop " << its.getRule(simpleLoop));
        its.removeRule(simpleLoop);
    }

    // Remove all incoming transitions that were successfully chained
    // FIXME: Find out why?! Is this really what the code below does?
    for (TransIdx toRemove : successfullyChained) {
        debugChain("removing transition " << its.getRule(toRemove));
        its.removeRule(toRemove);
    }

    // TODO: Figure out what this code is actually supposed to do...
//    for (const std::pair<TransIndex,bool> &pair : transitions) {
//        if (pair.second && !(addTransitionToSkipLoops.count(node) > 0)) {
//            debugChain("removing transition " << pair.first);
//            removeTrans(pair.first);
//        }
//    }

    return true;
}


bool Chaining::chainSimpleLoops(LinearITSProblem &its) {
    Timing::Scope timer(Timing::Contract);
    Stats::addStep("Chaining::chainSimpleLoops");

    bool res = false;
    for (NodeIndex node : its.getLocations()) {
        if (!its.getTransitionsFromTo(node, node).empty()) {
            if (chainSimpleLoopsAt(its, node)) {
                res = true;
            }

            if (Timeout::soft()) {
                return res;
            }
        }
    }

    return res;
}
