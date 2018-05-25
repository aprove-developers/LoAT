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


bool Pruning::compareRules(const Rule &a, const Rule &b, bool compareRhss) {
    const GuardList &guardA = a.getGuard();
    const GuardList &guardB = b.getGuard();

    // Some trivial syntactic checks
    if (guardA.size() != guardB.size()) return false;
    if (compareRhss && a.rhsCount() != b.rhsCount()) return false;

    // Costs have to be equal up to a numeric constant
    if (!GiNaC::is_a<GiNaC::numeric>(a.getCost() - b.getCost())) return false;

    // All right-hand sides have to match exactly
    if (compareRhss) {
        for (int i=0; i < a.rhsCount(); ++i) {
            const UpdateMap &updateA = a.getUpdate(i);
            const UpdateMap &updateB = b.getUpdate(i);

            if (a.getRhsLoc(i) != b.getRhsLoc(i)) return false;
            if (updateA.size() != updateB.size()) return false;

            // update has to be fully equal (one inclusion suffices, since the size is equal)
            for (const auto &itA : updateA) {
                auto itB = updateB.find(itA.first);
                if (itB == updateB.end()) return false;
                if (!itB->second.is_equal(itA.second)) return false;
            }
        }
    }

    // Guard has to be fully equal (including the ordering)
    for (int i=0; i < guardA.size(); ++i) {
        if (!guardA[i].is_equal(guardB[i])) return false;
    }
    // TODO: Replace the guard check by a ExprSymbolSet to ignore ordering?

    return true;
}


template <typename Container>
bool Pruning::removeDuplicateRules(ITSProblem &its, const Container &trans, bool compareRhss) {
    set<TransIdx> toRemove;

    for (auto i = trans.begin(); i != trans.end(); ++i) {
        for (auto j = i; ++j != trans.end(); /**/) {
            TransIdx idxA = *i;
            TransIdx idxB = *j;

            const Rule &ruleA = its.getRule(idxA);
            const Rule &ruleB = its.getRule(idxB);

            // if rules are identical up to cost, keep the one with the higher cost
            if (compareRules(ruleA, ruleB, compareRhss)) {
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


bool Pruning::removeUnsatInitialRules(ITSProblem &its) {
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


bool Pruning::pruneParallelRules(ITSProblem &its) {
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
                    const Rule &rule = its.getRule(parallel[idx]);

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

                // Remove all rules except for the ones in keep, add a dummy rule if there was one before
                // Note that for nonlinear rules, we only remove edges (so only single rhss), not the entire rule
                for (TransIdx rule : parallel) {
                    if (keep.count(rule) == 0) {
                        Stats::add(Stats::PruneRemove);
                        debugPrune("  removing all right-hand sides of " << rule << " from location " << pre << " to " << node);

                        auto optRule = its.getRule(rule).stripRhsLocation(node);
                        if (optRule) {
                            its.addRule(optRule.get());
                        }

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
static bool removeConstLeafs(ITSProblem &its, LocationIdx node, set<LocationIdx> &visited) {
    if (!visited.insert(node).second) return false; // already present

    // for brevity only
    auto isLeaf = [&its](LocationIdx loc){ return !its.hasTransitionsFrom(loc); };
    auto isLeafRhs = [&](const RuleRhs &rhs){ return isLeaf(rhs.getLoc()); };

    bool changed = false;
    for (LocationIdx next : its.getSuccessorLocations(node)) {
        // recurse first
        changed = removeConstLeafs(its, next, visited) || changed;

        // If next is (now) a leaf, rules leading to next are candidates for removal
        if (isLeaf(next)) {
            for (TransIdx ruleIdx : its.getTransitionsFromTo(node, next)) {
                const Rule &rule = its.getRule(ruleIdx);

                // only remove rules with constant complexity
                if (rule.getCost().getComplexity() > Complexity::Const) continue;

                // only remove rules where _all_ right-hand sides lead to leaves
                if (rule.rhsCount() == 1 || std::all_of(rule.rhsBegin(), rule.rhsEnd(), isLeafRhs)) {
                    debugPrune("  removing constant leaf rule: " << ruleIdx);
                    its.removeRule(ruleIdx);
                    changed = true;
                }
            }

            // If we removed all rules to the leaf, we can safely delete it
            if (!its.hasTransitionsTo(next)) {
                debugPrune("  removing isolated sink: " << next);
                its.removeOnlyLocation(next);
            }
        }
    }

    return changed;
}


bool Pruning::removeLeafsAndUnreachable(ITSProblem &its) {
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


// instantiate templates (since the implementation is not in the header file)
template bool Pruning::removeDuplicateRules(ITSProblem &, const std::vector<TransIdx> &, bool);
template bool Pruning::removeDuplicateRules(ITSProblem &, const std::set<TransIdx> &, bool);
