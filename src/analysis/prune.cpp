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

#include "global.h"
#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"

#include "its/itsproblem.h"

#include "z3/z3toolbox.h"
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



template <typename Container>
bool Pruning::removeUnsatRules(ITSProblem &its, const Container &trans) {
    bool changed = false;

    for (TransIdx rule : trans) {
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

    // To compare rules, we store a tuple of the rule's index, its complexity and the number of inftyVars
    // (see ComplexityResult for the latter). We first compare the complexity, then the number of inftyVars.
    typedef tuple<TransIdx,Complexity,int> TransCpx;
    auto comp = [](TransCpx a, TransCpx b) { return get<1>(a) < get<1>(b) || (get<1>(a) == get<1>(b) && get<2>(a) < get<2>(b)); };

    bool changed = false;
    for (LocationIdx node : its.getLocations()) {
        for (LocationIdx pre : its.getPredecessorLocations(node)) {
            if (Timeout::soft()) return changed;

            // First remove duplicates (this is rather cheap)
            removeDuplicateRules(its, its.getTransitionsFromTo(pre, node));

            // Then prune rules by only keeping the "best" ones (heuristically)
            const vector<TransIdx> &parallel = its.getTransitionsFromTo(pre, node);

            if (parallel.size() > Config::Prune::MaxParallelRules) {
                std::vector<std::tuple<TransIdx, Complexity, int>> queue;

                for (int i=0; i < parallel.size(); ++i) {
                    // alternating iteration (idx=0,n-1,1,n-2,...) that might avoid choosing similar edges
                    int idx = (i % 2 == 0) ? i/2 : parallel.size()-1-i/2;

                    TransIdx ruleIdx = parallel[idx];
                    const Rule &rule = its.getRule(parallel[idx]);

                    const Complexity &toBeat = queue.size() >= Config::Prune::MaxParallelRules ?
                            std::get<1>(queue.at(Config::Prune::MaxParallelRules - 1)) :
                            Complexity::Const;
                    // compute the complexity (real check using asymptotic bounds) and store in priority queue
                    auto res = AsymptoticBound::determineComplexityViaSMT(
                            its,
                            rule.getGuard(),
                            rule.getCost(),
                            false,
                            toBeat);
                    queue.emplace_back(make_tuple(ruleIdx, res.cpx, res.inftyVars));
                    std::sort(queue.rbegin(), queue.rend(), comp);
                    if (queue.size() > Config::Prune::MaxParallelRules) {
                        queue.pop_back();
                    }
                    if (Timeout::soft()) return changed;
                }

                // Keep only the top elements of the queue
                set<TransIdx> keep;
                auto it = queue.begin();
                for (int i=0; i < Config::Prune::MaxParallelRules && it < queue.end(); ++i) {
                    keep.insert(get<0>(*it));
                    it++;
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


/**
 * Helper for removeSinkRhss.
 * Deletes all rhss of the given rule that lead to the given location.
 * If all rhss lead to loc, then the rule is completely deleted (if it has constant complexity)
 * or a dummy rhs is added (if the rule has more than constant complexity).
 * @return true iff the ITS was modified
 */
static bool partialDeletion(ITSProblem &its, TransIdx ruleIdx, LocationIdx loc) {
    const Rule &rule = its.getRule(ruleIdx);
    assert(its.getTransitionTargets(ruleIdx).count(loc) > 0); // should only call this if we can delete something

    // If the rule only has one rhs, we do not change it (this ensures termination of the overall algorithm)
    if (rule.isLinear()) {
        return false;
    }

    // Replace the rule by a stripped rule (without rhss leading to loc), if possible
    auto optRule = rule.stripRhsLocation(loc);
    if (optRule) {
        TransIdx newIdx = its.addRule(optRule.get());
        (void)newIdx; // suppress compiler warning if debugging is disabled
        debugPrune("Partial deletion: Added stripped rule " << newIdx << " (for rule " << ruleIdx << ")");
    }

    // If all rhss would be deleted, we still keep the rule if it has an interesting complexity.
    if (!optRule) {
        if (rule.getCost().getComplexity() > Complexity::Const) {
            // Note that it is only sound to add a dummy transition to loc if loc is a sink location.
            // This should be the case when partialDeletion is called, at least for the current implementation.
            assert(!its.hasTransitionsFrom(loc));
            TransIdx newIdx = its.addRule(rule.replaceRhssBySink(loc));
            (void)newIdx; // suppress compiler warning if debugging is disabled
            debugPrune("Partial deletion: Added dummy rule " << newIdx << " (for rule " << ruleIdx << ")");
        }
    }

    // Remove the original rule
    its.removeRule(ruleIdx);
    return true;
}


// remove edges to locations without outdegree 0 (sink)
bool Pruning::removeSinkRhss(ITSProblem &its) {
    bool changed = false;

    for (LocationIdx node : its.getLocations()) {
        // if the location is a sink, remove it from all rules
        if (!its.hasTransitionsFrom(node)) {
            debugPrune("Applying partial deletion to sink location: " << node);
            for (TransIdx rule : its.getTransitionsTo(node)) {
                changed = partialDeletion(its, rule, node) || changed;
            }

            // if we could remove all incoming rules, we can remove the sink
            if (!its.isInitialLocation(node) && !its.hasTransitionsTo(node)) {
                debugPrune("Removing unreachable sink (after partial deletion): " << node);
                its.removeOnlyLocation(node);
            }
        }
    }

    return changed;
}


// instantiate templates (since the implementation is not in the header file)
template bool Pruning::removeDuplicateRules(ITSProblem &, const std::vector<TransIdx> &, bool);
template bool Pruning::removeDuplicateRules(ITSProblem &, const std::set<TransIdx> &, bool);
template bool Pruning::removeUnsatRules(ITSProblem &, const std::vector<TransIdx> &);
template bool Pruning::removeUnsatRules(ITSProblem &, const std::set<TransIdx> &);
