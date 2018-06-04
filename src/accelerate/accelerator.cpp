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

#include "accelerator.h"

#include "analysis/preprocess.h"
#include "recurrence/recurrence.h"
#include "meter/metering.h"
#include "z3/z3toolbox.h"

#include "its/rule.h"
#include "its/export.h"

// TODO: Move these to proper folders after refactoring
#include "analysis/chain.h"
#include "analysis/prune.h"

#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"
#include "forward.h"
#include "backwardacceleration.h"

#include <queue>


using namespace std;
using boost::optional;

namespace Forward = ForwardAcceleration;
using Backward = BackwardAcceleration;


Accelerator::Accelerator(ITSProblem &its, LocationIdx loc, std::set<TransIdx> &resultingRules)
    : its(its), targetLoc(loc), resultingRules(resultingRules)
{
    // We need a sink location for INF rules and nonlinear rules
    // To avoid too many parallel rules (which would then be pruned), we use a new one for each run
    sinkLoc = its.addLocation();
}


TransIdx Accelerator::addResultingRule(Rule rule) {
    TransIdx idx = its.addRule(rule);
    resultingRules.insert(idx);
    return idx;
}


// #####################
// ##  Preprocessing  ##
// #####################

bool Accelerator::simplifySimpleLoops() {
    bool res = false;
    vector<TransIdx> loops = its.getSimpleLoopsAt(targetLoc);

    // Simplify all all simple loops.
    // This is especially useful to eliminate temporary variables before metering.
    for (TransIdx loop : loops) {
        res |= Preprocess::simplifyRule(its, its.getRuleMut(loop));
        if (Timeout::soft()) return res;
    }

    // Remove duplicate rules (does not happen frequently, but the syntactical check should be cheap anyway)
    if (Pruning::removeDuplicateRules(its, loops)) {
        res = true;
        debugAccel("Removed some duplicate simple loops");
    }

    return res;
}


// ########################
// ##  Nesting of Loops  ##
// ########################

bool Accelerator::canNest(const LinearRule &inner, const LinearRule &outer) const {
    // Collect all variables appearing in the inner guard
    ExprSymbolSet innerGuardSyms;
    for (const Expression &ex : inner.getGuard()) {
        ex.collectVariables(innerGuardSyms);
    }

    // If any of these variables is affected by the outer update,
    // then applying the outer loop can affect the inner loop's condition,
    // so it might be possible to execute the inner loop again (and thus nesting might work).
    for (const auto &it : outer.getUpdate()) {
        ExprSymbol updated = its.getGinacSymbol(it.first);
        if (innerGuardSyms.count(updated) > 0) {
            return true;
        }
    }

    // TODO: This check can be improved, e.g. by issuing a z3 query of the form
    // TODO:  (not IG) and OG  ==>  outer_update(IG)  is valid.
    // TODO: So if the inner guard does no longer hold (and the outer one does),
    // TODO: Then applying the outer update will make the inner guard applicable again.

    // TODO: Note that this check is actually a severe restriction of the current check!
    // TODO: Need some statistics on this.
    return false;
}


void Accelerator::addNestedRule(const Forward::MeteredRule &metered, const LinearRule &chain,
                                TransIdx inner, TransIdx outer)
{
    // Add the new rule
    TransIdx added = addResultingRule(metered.rule);

    // The outer rule was accelerated (after nesting), so we do not need to keep it anymore
    keepRules.erase(outer);

    proofout << "Nested simple loops " << outer << " (outer loop) and " << inner;
    proofout << " (inner loop) with " << metered.info;
    proofout << ", resulting in the new rules: " << added;

    // Try to combine chain and the accelerated loop
    auto chained = Chaining::chainRules(its, chain, metered.rule);
    if (chained) {
        TransIdx added = addResultingRule(chained.get());
        proofout << ", " << added;
    }
    proofout << "." << endl;
}


bool Accelerator::nestRules(const InnerCandidate &inner, const OuterCandidate &outer) {
    bool res = false;
    const LinearRule innerRule = its.getLinearRule(inner.newRule);
    const LinearRule outerRule = its.getLinearRule(outer.oldRule);

    // Avoid nesting a loop with its original transition or itself
    if (inner.oldRule == outer.oldRule || inner.newRule == outer.oldRule) {
        return false;
    }

    // Skip inner loops with constant costs
    if (innerRule.getCost().getComplexity() == Complexity::Const) {
        return false;
    }

    // Check by some heuristic if it makes sense to nest inner and outer
    if (!canNest(innerRule, outerRule)) {
        return false;
    }

    // We only consider nesting successful if it increases the complexity
    Complexity oldCpx = innerRule.getCost().getComplexity();

    // Lambda that performs the nesting/acceleration.
    // If successful, also tries to chain the second rule in front of the accelerated (nested) rule.
    // This can improve results, since we do not know which loop is executed first.
    auto nestAndChain = [&](const LinearRule &first, const LinearRule &second) -> bool {
        auto optNested = Chaining::chainRules(its, first, second);
        if (optNested) {
            Rule nestedRule = optNested.get();

            // Simplify the rule again (chaining can introduce many useless constraints)
            Preprocess::simplifyRule(its, nestedRule);

            // TODO: Use full accelerate with heuristics (and backward acceleration)?
            // TODO: We probably don't want to use backward accel here, as it often complicates the rule...
            auto optAccel = Forward::accelerateFast(its, nestedRule, sinkLoc);
            if (optAccel) {
                Forward::MeteredRule accelRule = optAccel.get();

                if (accelRule.rule.getCost().getComplexity() > oldCpx) {
                    addNestedRule(accelRule, second, inner.newRule, outer.oldRule);
                    return true;
                }
            }
        }
        return false;
    };

    // Try to nest, executing inner loop first (and try to chain outerRule in front)
    res |= nestAndChain(innerRule, outerRule);

    // Try to nest, executing outer loop first (and try to chain innerRule in front).
    // If the previous nesting was already successful, it probably covers most execution
    // paths already, since "nestAndChain" also tries to chain the second rule in front.
    if (!res) {
        res |= nestAndChain(outerRule, innerRule);
    }

    return res;
}


void Accelerator::performNesting(vector<InnerCandidate> inner, vector<OuterCandidate> outer) {
    debugAccel("Nesting with " << inner.size() << " inner and " << outer.size() << " outer candidates");
    bool changed = false;

    // Try to combine previously identified inner and outer candidates via chaining,
    // then try to accelerate the resulting rule
    for (const InnerCandidate &in : inner) {
        for (const OuterCandidate &out : outer) {
            changed |= nestRules(in, out);
            if (Timeout::soft()) return;
        }
    }
}


// ############################
// ## Removal (cleaning up)  ##
// ############################

void Accelerator::removeOldLoops(const vector<TransIdx> &loops) {
    // Remove all old loops, unless we have decided to keep them
    proofout << "Removing the simple loops:";
    for (TransIdx loop : loops) {
        if (keepRules.count(loop) == 0) {
            proofout << " " << loop;
            its.removeRule(loop);
        }
    }
    proofout << "." << endl;

    // In some cases, two loops can yield similar accelerated rules, so we prune duplicates
    // and have to remove rules that were removed from resultingRules.
    if (Pruning::removeDuplicateRules(its, resultingRules)) {
        proofout << "Also removing duplicate rules:";
        auto it = resultingRules.begin();
        while (it != resultingRules.end()) {
            if (its.hasRule(*it)) {
                it++;
            } else {
                proofout << " " << (*it);
                it = resultingRules.erase(it);
            }
        }
        proofout << "." << endl;
    }
}


// #####################
// ## Main algorithm  ##
// #####################

void Accelerator::run() {
    // Simplifying rules might make it easier to find metering functions
    if (simplifySimpleLoops()) {
        proofout << "Simplified some of the simple loops (and removed duplicate rules)." << endl;
    }

    // Since we might add accelerated loops, we store the list of loops before acceleration
    vector<TransIdx> loops = its.getSimpleLoopsAt(targetLoc);
    if (loops.empty()) {
        proofout << "No simple loops left to accelerate." << endl;
        return; // may happen if rules get removed in simplifySimpleLoops
    }

    // Proof output
    proofout << "Accelerating the following rules:" << endl;
    for (TransIdx loop : loops) {
        ITSExport::printLabeledRule(loop, its, proofout);
    }

    // While accelerating, collect rules that might be feasible for nesting
    // Inner candidates are accelerated rules, since they correspond to a loop within another loop.
    // Outer candidates are loops that cannot be accelerated on their own (because they are missing their inner loop)
    vector<InnerCandidate> innerCandidates;
    vector<OuterCandidate> outerCandidates;

    // Try to accelerate all loops
    for (TransIdx loop : loops) {
        if (Timeout::soft()) return;

        // rules with INF cost should never be self loops (they should always lead to sink states)
        // this assertion is mostly relevant during refactoring.
        // TODO: remove this later
        assert(!its.getRule(loop).getCost().isInfSymbol());

        // Forward acceleration
        Forward::Result res = Forward::accelerate(its, its.getRule(loop), sinkLoc);

        // Try backward acceleration only if forward acceleration failed,
        // or if it only succeeded by restricting the guard. In this case,
        // we keep the rules from forward and just add the ones from backward acceleration.
        if (res.result != Forward::Success && its.getRule(loop).isLinear()) {
            auto optRules = Backward::accelerate(its, its.getLinearRule(loop));
            if (optRules) {
                res.result = Forward::Success;
                for (LinearRule rule : optRules.get()) {
                    res.rules.emplace_back("backward acceleration", rule);
                }
            }
        }

        // Interpret the results, add new rules
        switch (res.result) {
            case Forward::TooComplicated:
                // rules is probably not relevant for nesting
                keepRules.insert(loop);
                proofout << "Found no metering function for rule " << loop << " (rule is too complicated)." << endl;
                break;

            case Forward::NoMetering:
                if (its.getRule(loop).isLinear()) {
                    outerCandidates.push_back({loop, "NoMetering"});
                }
                keepRules.insert(loop);
                proofout << "Found no metering function for rule " << loop << "." << endl;
                break;

            case Forward::SuccessWithRestriction:
                // If we only succeed by extending the rule's guard, we make the rule more restrictive
                // and can thus lose execution paths. So we also keep the original, unaccelerated rule.
                keepRules.insert(loop);

                // fall-through

            case Forward::Success:
            {
                bool isNonterm = false;

                // Add accelerated rules, also mark them as inner nesting candidates
                for (Forward::MeteredRule accel : res.rules) {
                    TransIdx added = addResultingRule(accel.rule);
                    proofout << "Accelerated rule " << loop << " with " << accel.info;
                    proofout << ", yielding the new rule " << added << "." << endl;

                    if (accel.rule.isSimpleLoop()) {
                        // accel.rule is a simple loop iff the original was linear and not non-terminating.
                        innerCandidates.push_back(InnerCandidate{.oldRule=loop,.newRule=added});
                    }

                    isNonterm = isNonterm || accel.rule.getCost().isInfSymbol();
                }

                // If the guard was modified, the original rule might not be non-terminating
                if (res.result == Forward::SuccessWithRestriction) {
                    isNonterm = false;
                }

                // The original rule could still be an outer loop for nesting,
                // unless it is non-terminating (so nesting will not improve the result).
                // TODO: Check via benchmarks if this ever happens (or if we only waste time here)
                if (its.getRule(loop).isLinear() && !isNonterm) {
                    outerCandidates.push_back({loop, "Ranked"});
                }
                break;
            }
        }
    }

    // Nesting
    performNesting(std::move(innerCandidates), std::move(outerCandidates));
    if (Timeout::soft()) return;

    // TODO: Do we also want to chain accelerated rules with themselves?
    // TODO: We could do this if the number of accelerated rules is below a certain threshold
    // TODO: (e.g. similar to PRUNE_MAX_PARALLEL_RULES)

    // Simplify the guards of accelerated rules.
    // Especially backward acceleration and nesting can introduce superfluous constraints.
    for (TransIdx rule : resultingRules) {
        Preprocess::simplifyGuard(its.getRuleMut(rule).getGuardMut());
    }

    // If we failed for any rule, we add a dummy rule to simulate the effect of not executing any loop.
    // The reason is that we later chain the accelerated rules with incoming rules. So we only allow
    // execution paths that take one of the accelerated (or kept) rules, but we do not allow an execution
    // path which does not execute any loop. By adding a dummy loop, we allow such execution paths.
    // Since this quickly leads to rule explosion, we only do this if we failed to accelerate some rules.
    if (!keepRules.empty()) {
        TransIdx added = addResultingRule(Rule::dummyRule(targetLoc, targetLoc));
        proofout << "Adding an empty simple loop: " << added << "." << endl;
    }

    // Keep rules for which acceleration failed (maybe these rules are in fact not loops)
    for (TransIdx rule : keepRules) {
        resultingRules.insert(rule);
    }

    // Remove old rules
    removeOldLoops(loops);
}


// #######################
// ## Public interface  ##
// #######################

bool Accelerator::accelerateSimpleLoops(ITSProblem &its, LocationIdx loc, std::set<TransIdx> &resultingRules) {
    if (its.getSimpleLoopsAt(loc).empty()) {
        return false;
    }

    proofout << endl;
    proofout.setLineStyle(ProofOutput::Headline);
    proofout << "Accelerating simple loops of location " << loc << "." << endl;
    proofout.increaseIndention();

    // Accelerate all loops (includes optimizations like nesting)
    Accelerator accel(its, loc, resultingRules);
    accel.run();

    proofout.decreaseIndention();
    return true;
}
