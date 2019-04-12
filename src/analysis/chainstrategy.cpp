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

#include "chainstrategy.hpp"

#include "chain.hpp"
#include "preprocess.hpp"

#include "../debug.hpp"
#include "../util/stats.hpp"
#include "../util/timing.hpp"
#include "../util/timeout.hpp"


using namespace std;


// ############################
// ##  Location Elimination  ##
// ############################

/**
 * Eliminates the given location by chaining every incoming with every outgoing transition.
 *
 * @note The given location must not have any self-loops (simple or non-simple),
 * unless allowSelfloops is true (but even then, it must not have simple loops!).
 *
 * If keepUnchainable is true and some incoming transition T cannot be chained with at least one outgoing transition,
 * then a new dummy location is inserted and T is kept, connecting its its old source to the new dummy location.
 * However, this is only done if the cost of T is more than constant.
 *
 * The old location is removed, together with all old transitions. So if an outgoing transition cannot be chained
 * with any incoming transition, it will simply be removed.
 */
static void eliminateLocationByChaining(ITSProblem &its, LocationIdx loc,
                                        bool keepUnchainable, bool allowSelfloops = false)
{
    set<TransIdx> keepRules;
    debugChain("  eliminating location " << loc << " by chaining (keep unchainable: " << keepUnchainable << ")");

    bool wasProofEnabled = proofout.setEnabled(Config::Output::ProofChain);
    proofout.headline("Eliminating location " + its.getPrintableLocationName(loc) + " by chaining:");
    proofout.increaseIndention();

    // Chain all pairs of in- and outgoing rules
    for (TransIdx in : its.getTransitionsTo(loc)) {
        bool wasChainedWithAll = true;
        const Rule &inRule = its.getRule(in);

        // We usually require that loc doesn't have any self-loops (since we would destroy the self-loop by chaining).
        // E.g. chaining f -> g, g -> g would result in f -> g without the self-loop.
        assert(allowSelfloops || inRule.getLhsLoc() != loc);

        // If we allow self-loops, we ignore them for incoming rules,
        // since the resulting chained rule would in the end be deleted (together with loc) anyway.
        if (inRule.getLhsLoc() == loc) continue;

        for (TransIdx out : its.getTransitionsFrom(loc)) {
            auto optRule = Chaining::chainRules(its, inRule, its.getRule(out));
            if (optRule) {
                // If we allow self loops at loc, then chained rules may still lead to loc,
                // e.g. if h -> f and f -> f,g are chained to h -> f,g (where f is loc).
                // Since we want to eliminate loc, we remove all rhss leading to loc (e.g. h -> g).
                if (allowSelfloops) {
                    optRule = optRule.get().stripRhsLocation(loc);
                    assert(optRule); // this only happens for simple loops, which we disallow
                }

                // Simplify the guard (chaining often introduces trivial constraints)
                Rule newRule = optRule.get();
                Preprocess::simplifyGuard(newRule.getGuardMut());

                TransIdx added = its.addRule(newRule);
                debugChain("    chained " << in << " and " << out << " to new rule: " << added);
                proofout << "Chained rules " << in << " and " << out << " to new rule " << added << "." << endl;

            } else {
                wasChainedWithAll = false;
                debugChain("    failed to chain " << in << " and " << out);
                proofout << "Failed to chain rules " << in << " and " << out << "." << endl;
            }
        }

        if (keepUnchainable && !wasChainedWithAll) {
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
            auto newRule = its.getRule(trans).stripRhsLocation(loc);
            if (newRule) {
                // In case of nonlinear rules, we can simply delete all rhss leading to loc, but keep the other ones
                TransIdx added = its.addRule(newRule.get());
                debugChain("    keeping rule " << trans << " by adding stripped rule: " << added);
                proofout << "Keeping rule " << trans << " after partial deletion, yielding new rule " << added << "." << endl;
            } else {
                // If all rhss lead to loc (for instance if the rule is linear), we add a new dummy rhs
                TransIdx added = its.addRule(its.getRule(trans).replaceRhssBySink(dummyLoc));
                debugChain("    keeping rule " << trans << " by adding dummy rule: " << added);
                proofout << "Keeping rule " << trans << " by adding a dummy rule " << added << "." << endl;
            }
        }
    }

    // Remove loc and all incoming/outgoing rules.
    // Note that all rules have already been chained (or backed up), so removing these rules is ok.
    its.removeLocationAndRules(loc);

    proofout.decreaseIndention();
    proofout.setEnabled(wasProofEnabled);
}


// ##############################
// ##  Helpers for Strategies  ##
// ##############################

/**
 * Implementation of callRepeatedlyOnEachNode
 */
template <typename F>
static bool callOnEachNodeImpl(ITSProblem &its, F function, LocationIdx node, bool repeat, set<LocationIdx> &visited) {
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

    } while (repeat && changed);

    // Continue with the successors of the current node (DFS traversal)
    for (LocationIdx next : its.getSuccessorLocations(node)) {
        bool changed = callOnEachNodeImpl(its, function, next, repeat, visited);
        changedOverall = changedOverall || changed;

        if (Timeout::soft()) {
            return changedOverall;
        }
    }

    return changedOverall;
}


/**
 * A dfs traversal through the its's graph, starting in the initial location,
 * calling the given function for each node.
 *
 * The given function must return a boolean value (a "changed" flag).
 * If repeat is true, the function is called several times on every
 * visited node, as long as it returns true. If it returns false
 * (or if repeat is false), the DFS continues with the next node.
 * The function is allowed to modify the ITS (and thus the graph).
 *
 * The given set `visited` should be empty.
 *
 * @returns true iff at least one call of the given function returned true.
 */
template <typename F>
static bool callOnEachNode(ITSProblem &its, F function, bool repeat) {
    set<LocationIdx> visited;
    return callOnEachNodeImpl(its, function, its.getInitialLocation(), repeat, visited);
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


// ###########################
// ##  Chaining Strategies  ##
// ###########################

bool Chaining::chainLinearPaths(ITSProblem &its) {
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
            }
        }
        return changed;
    };

    debugChain("Chaining linear paths");
    return callOnEachNode(its, implementation, true);
}


bool Chaining::chainTreePaths(ITSProblem &its) {
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
            if (its.hasTransitionsFrom(succ)) {
                eliminateLocationByChaining(its, succ, true);
                changed = true;
            }

            if (Timeout::soft()) break;
        }
        return changed;
    };

    debugChain("Chaining tree paths");

    // To avoid rule explosion, the implementation is only called once per node.
    // Example: path f -> g -> h -> u -> ... When called on f, g is eliminated.
    // We then call the implementation on h (f's new child), which may eliminate u.
    // This avoids the exponential blowup, so we can first accelerate rules or
    // prune rules before calling this method again.
    return callOnEachNode(its, implementation, false);
}


/**
 * Implementation of eliminateALocation
 */
static bool eliminateALocationImpl(ITSProblem &its, LocationIdx node, set<LocationIdx> &visited, string &eliminated) {
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
            if (eliminateALocationImpl(its, succ, visited, eliminated)) {
                return true;
            }

            if (Timeout::soft()) {
                return false;
            }
        }
        return false;
    }

    // Otherwise, we can eliminate node
    eliminated = its.getPrintableLocationName(node);
    debugChain("  found location to eliminate: " << node);
    eliminateLocationByChaining(its, node, true, true);
    return true;
}


bool Chaining::eliminateALocation(ITSProblem &its, string &eliminatedLocation) {
    debugChain("Trying to eliminate a location");

    set<LocationIdx> visited;
    return eliminateALocationImpl(its, its.getInitialLocation(), visited, eliminatedLocation);
}


// ###################################
// ##  Chaining after Acceleration  ##
// ###################################

bool Chaining::chainAcceleratedRules(ITSProblem &its, const set<TransIdx> &acceleratedRules) {
    if (acceleratedRules.empty()) return false;
    set<TransIdx> successfullyChained;

    bool wasProofEnabled = proofout.setEnabled(Config::Output::ProofChain);
    proofout.headline("Chaining " + to_string(acceleratedRules.size()) + " accelerated rules with incoming rules:");
    proofout.increaseIndention();

    // Find all lhs locations of accelerated rules, so we can iterate over them.
    // If we would iterate over acceleratedRules directly, we might consider an lhs location twice,
    // so we would use chained rules from the first visit as incoming rules for the second visit.
    set<LocationIdx> nodes;
    for (TransIdx accel : acceleratedRules) {
        nodes.insert(its.getRule(accel).getLhsLoc());
    }

    for (LocationIdx node : nodes) {
        // We only query the incoming transitions once, before adding new rules starting at node
        set<TransIdx> incomingTransitions = its.getTransitionsTo(node);

        for (TransIdx accel : its.getTransitionsFrom(node)) {
            if (Timeout::soft()) break;

            // Only chain accelerated rules
            if (acceleratedRules.count(accel) == 0) continue;
            const Rule &accelRule = its.getRule(accel);
            debugChain("Chaining accelerated rule " << accel);

            for (TransIdx incoming : incomingTransitions) {
                // Do not chain with another accelerated rule
                if (acceleratedRules.count(incoming) > 0) continue;

                // Do not chain with incoming loops that are themselves self-loops at node
                // (no matter if they are simple or not)
                const Rule &incomingRule = its.getRule(incoming);
                if (incomingRule.getLhsLoc() == node) continue;

                auto optRule = Chaining::chainRules(its, incomingRule, accelRule);
                if (optRule) {
                    // Simplify the rule (can help to eliminate temporary variables of the metering function)
                    Rule newRule = optRule.get();
                    Preprocess::simplifyRule(its, newRule);

                    // Add the chained rule
                    TransIdx added = its.addRule(newRule);
                    debugChain("  chained incoming rule " << incoming << " with " << accel << ", resulting in new rule: " << added);
                    proofout << "Chained incoming rule " << incoming << " with accelerated rule " << accel << " to new rule " << added << "." << endl;
                    successfullyChained.insert(incoming);
                }
            }

            debugChain("  removing accelerated rule " << accel);
            proofout << "Removing accelerated rule " << accel << "." << endl;
            its.removeRule(accel);
        }
    }

    // Removing chained incoming rules may help to avoid too many rules.
    // However, we also lose execution paths (especially if there are more loops, which are not simple).
    if (!Config::Chain::KeepIncomingInChainAccelerated) {
        for (TransIdx toRemove : successfullyChained) {
            debugChain("  removing chained incoming rule " << its.getRule(toRemove));
            proofout << "Removing incoming rule " << toRemove << " (after successful chaining)." << endl;
            its.removeRule(toRemove);
        }
    }

    proofout.decreaseIndention();
    proofout.setEnabled(wasProofEnabled);

    return !acceleratedRules.empty();
}
