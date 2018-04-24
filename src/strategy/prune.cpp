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

#include "prune.h"

#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"

#include "its/itsproblem.h"

#include "z3/z3toolbox.h"
#include "infinity.h"
#include "asymptotic/asymptoticbound.h"

#include <queue>


using namespace std;


bool Pruning::compareRules(const LinearRule &a, const LinearRule &b, bool compareUpdate) {
    const GuardList &guardA = a.getGuard();
    const GuardList &guardB = b.getGuard();
    const UpdateMap &updateA = a.getUpdate();
    const UpdateMap &updateB = b.getUpdate();

    // Some trivial syntactic checks
    if (guardA.size() != guardB.size()) return false;
    if (compareUpdate && updateA.size() != updateB.size()) return false;

    // Costs have to be equal up to a numeric constant
    if (!GiNaC::is_a<GiNaC::numeric>(a.getCost() - b.getCost())) return false;

    // Update has to be fully equal (this check suffices, since the size is equal)
    if (compareUpdate) {
        for (const auto &itA : updateA) {
            auto itB = updateB.find(itA.first);
            if (itB == updateB.end()) return false;
            if (!itB->second.is_equal(itA.second)) return false;
        }
    }

    // Guard has to be fully equal (including the ordering)
    for (int i=0; i < guardA.size(); ++i) {
        if (!guardA[i].is_equal(guardB[i])) return false;
    }
    // TODO: Replace the guard check by a ExprSymbolSet to ignore ordering?

    return true;
}


bool Pruning::removeDuplicateRules(LinearITSProblem &its, const vector<TransIdx> &trans, bool compareUpdate) {
    set<TransIdx> toRemove;

    for (int i=0; i < trans.size(); ++i) {
        for (int j=i+1; j < trans.size(); ++j) {
            TransIdx idxA = trans[i];
            TransIdx idxB = trans[j];

            const LinearRule &ruleA = its.getRule(idxA);
            const LinearRule &ruleB = its.getRule(idxB);

            // if rules are identical up to cost, keep the one with the higher cost
            if (compareRules(ruleA, ruleB ,compareUpdate)) {
                if (GiNaC::ex_to<GiNaC::numeric>(ruleA.getCost() - ruleB.getCost()).is_positive()) {
                    toRemove.insert(idxB);
                } else {
                    toRemove.insert(idxA);
                    break; // do not remove trans[i] again
                }
            }
        }
    }

    for (TransIdx rule : toRemove) {
        debugPrune("Removing duplicate rule: " << rule);
        its.removeRule(rule);
    }

    return !toRemove.empty();
}


bool Pruning::removeUnsatInitialRules(LinearITSProblem &its) {
    bool changed = false;

    for (TransIdx rule : its.getTransitionsFrom(its.getInitialLocation())) {
        if (Z3Toolbox::checkAll(its.getRule(rule).getGuard()) == z3::unsat) {
            debugPrune("Removing unsat rule: " << rule);
            its.removeRule(rule);
            changed = true;
        }
    }

    return changed;
}


bool Pruning::pruneParallelRules(LinearITSProblem &its) {
    debugPrune("Pruning parallel rules");

    // FIXME: this method used to also call removeConstLeafsAndUnreachable (at the start)
    // FIXME: Remember to call this method wherever pruneParallelRules is called!
    //bool changed = removeConstLeafsAndUnreachable();

    // FIXME: Remember to check PRUNING_ENABLE and do Stats::addStep("Flowgraph::pruneTransitions");

    // To compare rules, we store a tuple of the rule's index, its complexity and the number of inftyVars
    // (see ComplexityResult for the latter). We first compare the complexity, then the number of inftyVars.
    typedef tuple<TransIdx,Complexity,int> TransCpx;
    auto comp = [](TransCpx a, TransCpx b) { return get<1>(a) < get<1>(b) || (get<1>(a) == get<1>(b) && get<2>(a) < get<2>(b)); };

    // We use a priority queue with the aforementioned comparison of rules (and vector as underlying container).
    typedef priority_queue<TransCpx, vector<TransCpx>, decltype(comp)> PriorityQueue;

    bool changed = false;
    for (LocationIdx node : its.getLocations()) {
        if (Timeout::soft()) break;

        for (LocationIdx pre : its.getPredecessorLocations(node)) {
            // First remove duplicates
            removeDuplicateRules(its, its.getTransitionsFromTo(pre, node));

            // Then prune rules by only keeping the "best" ones (heuristically)
            const vector<TransIdx> &parallel = its.getTransitionsFromTo(pre, node);

            if (parallel.size() > PRUNE_MAX_PARALLEL_TRANSITIONS) {
                PriorityQueue queue(comp);

                for (int i=0; i < parallel.size(); ++i) {
                    // alternating iteration (idx=0,n-1,1,n-2,...) that might avoid choosing similar edges
                    int idx = (i % 2 == 0) ? i/2 : parallel.size()-1-i/2;

                    TransIdx ruleIdx = parallel[idx];
                    const LinearRule &rule = its.getRule(parallel[idx]);

                    // compute the complexity (real check using asymptotic bounds) and store in priority queue
                    auto res = AsymptoticBound::determineComplexity(its, rule.getGuard(), rule.getCost(), false);
                    queue.push(make_tuple(ruleIdx, res.cpx, res.inftyVars));
                }

                // Keep only the top elements of the queue
                set<TransIdx> keep;
                for (int i=0; i < PRUNE_MAX_PARALLEL_TRANSITIONS; ++i) {
                    keep.insert(get<0>(queue.top()));
                    queue.pop();
                }

                // Check if there is an dummy rule (if there is, we want to keep an empty rule)
                bool hasDummy = false;
                for (TransIdx rule : parallel) {
                    if (its.getRule(rule).isDummyRule()) {
                        hasDummy = true;
                        break;
                    }
                }

                // Remove all rules except for the one in keep, add a dummy rule if there was one before
                for (TransIdx rule : parallel) {
                    if (keep.count(rule) == 0) {
                        Stats::add(Stats::PruneRemove);
                        debugPrune("  removing rule " << rule << " from location " << pre << " to " << node);
                        its.removeRule(rule);
                    }
                }
                if (hasDummy) {
                    debugPrune("  re-adding dummy rule from location " << pre << " to " << node);
                    its.addRule(LinearRule::dummyRule(pre, node));
                }

                changed = true;
            }
        }
    }
    return changed;
}


/**
 * Helper for removeLeafsAndUnreachable.
 * Performs a DFS and removes rules to leafs with constant complexity.
 * Returns true iff the ITS was modified.
 */
static bool removeConstLeafs(LinearITSProblem &its, LocationIdx node, set<LocationIdx> &visited) {
    if (!visited.insert(node).second) return false; // already present

    bool changed = false;
    for (LocationIdx next : its.getSuccessorLocations(node)) {
        // recurse first
        changed = removeConstLeafs(its, next, visited) || changed;

        // if next is (now) a leaf, remove all rules to next that have constant cost
        if (its.getTransitionsFrom(next).empty()) {
            for (TransIdx rule : its.getTransitionsFromTo(node, next)) {
                if (its.getRule(rule).getCost().getComplexity() == Complexity::Const) {
                    debugPrune("  removing constant leaf rule: " << rule);
                    its.removeRule(rule);
                    changed = true;
                }
            }
        }
    }

    return changed;
}


bool Pruning::removeLeafsAndUnreachable(LinearITSProblem &its) {
    set<LocationIdx> visited;
    debugPrune("Removing leafs and unreachable");

    // Remove rules to leafs if they do not give nontrivial complexity
    bool changed = removeConstLeafs(its, its.getInitialLocation(), visited);

    // Remove all nodes that have not been reached in the DFS traversal
    for (LocationIdx node : its.getLocations()) {
        if (visited.count(node) == 0) {
            debugPrune("  removing unreachable location: " << node);
            its.removeLocationAndRules(node);
            changed = true;
        }
    }

    return changed;
}



