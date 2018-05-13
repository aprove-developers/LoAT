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

#include "accelerate.h"

#include "preprocess.h"
#include "meter/recurrence.h"
#include "meter/metering.h"
#include "z3/z3toolbox.h"

#include "its/rule.h"
#include "its/export.h"

#include "chaining.h"

#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"

#include <queue>


using namespace std;
using boost::optional;



Accelerator::Accelerator(LinearITSProblem &its, LocationIdx loc)
        : its(its), targetLoc(loc) {
}



// #####################
// ##  Preprocessing  ##
// #####################


bool Accelerator::simplifyRule(VarMan &varMan, LinearRule &rule) {
    Timing::Scope(Timing::Preprocess); // FIXME: This is not pre-processing!
    return Preprocess::simplifyRule(varMan, rule);
}


// TODO: Move this to chaining, rename to chainAllPairs(its, vec<TransIdx>);
bool Accelerator::chainAllLoops(LinearITSProblem &its, LocationIdx loc) {
    bool changed = false;
    vector<TransIdx> loops = its.getTransitionsFromTo(loc, loc);
    debugAccel("Chaining all loops before acceleration");

    for (TransIdx first : loops) {
        for (TransIdx second : loops) {
            if (first == second) {
                continue;
            }

            auto chained = Chaining::chainRules(its, its.getRule(first), its.getRule(second));
            if (chained) {
                TransIdx added = its.addRule(chained.get());
                debugAccel("  chained rules " << first << " and " << second << ", resulting in new rule: " << added);
                changed = true;
            }
        }
    }

    return changed;
}



// #####################################
// ##  Acceleration, filling members  ##
// #####################################


bool Accelerator::handleMeteringResult(TransIdx ruleIdx, const LinearRule &rule, MeteringFinder::Result res) {
    // TODO: Add proof output also for failures

    if (res.result == MeteringFinder::ConflictVar) {
        // ConflictVar is just Unsat with more information
        res.result = MeteringFinder::Unsat;
        rulesWithConflictingVariables.push_back({ruleIdx, res.conflictVar});
    }

    switch (res.result) {
        case MeteringFinder::Unsat:
            Stats::add(Stats::SelfloopNoRank);
            debugAccel("Farkas unsat for rule " << ruleIdx);

            // Maybe the loop is just too difficult for us, so we allow to skip it (in the end)
            failedRulesNeedingEmptyLoop.insert(ruleIdx);

            // Maybe we can only find a metering function if we nest this loop with an accelerated inner loop,
            // or if we try to strengthen the guard
            outerNestingCandidates.push_back({ruleIdx});
            rulesWithUnsatMetering.insert(ruleIdx);

            // We cannot accelerate, so we just keep the unaccelerated rule
            keepRules.insert(ruleIdx);
            return false;

        case MeteringFinder::Nonlinear:
            Stats::add(Stats::SelfloopNoRank);
            debugAccel("Farkas nonlinear for rule " << ruleIdx);

            // Maybe the loop is just too difficult for us, so we allow to skip it (in the end)
            failedRulesNeedingEmptyLoop.insert(ruleIdx);

            // We cannot accelerate, so we just keep the unaccelerated rule
            keepRules.insert(ruleIdx);
            return false;

        case MeteringFinder::Unbounded:
            Stats::add(Stats::SelfloopInfinite);
            debugAccel("Farkas unbounded for rule " << ruleIdx);

            // In case we only got here in a second attempt (by some heuristic), clear some statistics
            failedRulesNeedingEmptyLoop.erase(ruleIdx);
            keepRules.erase(ruleIdx);

            // The rule is nonterminating. We can ignore the update, but the guard still has to be kept.
            {
                LinearRule newRule = rule;
                newRule.getCostMut() = Expression::InfSymbol;
                newRule.getUpdateMut().clear();

                TransIdx t = its.addRule(std::move(newRule));

                proofout << "Simple loop " << ruleIdx << " has unbounded runtime, ";
                proofout << "resulting in the new transition " << t << "." << endl;
            }
            return true;

        case MeteringFinder::Success:
            debugAccel("Farkas success, got " << res.metering << " for rule " << ruleIdx);
            {
                // The metering function might need additional guards
                LinearRule newRule = rule;
                if (res.integralConstraint) {
                    newRule.getGuardMut().push_back(res.integralConstraint.get());
                }

                // Compute new update and cost
                if (!Recurrence::calcIterated(its, newRule, res.metering)) {
                    Stats::add(Stats::SelfloopNoUpdate);

                    // Maybe the loop is just too difficult for us, so we allow to skip it (in the end)
                    failedRulesNeedingEmptyLoop.insert(ruleIdx);

                    // We cannot accelerate, so we just keep the unaccelerated rule
                    keepRules.insert(ruleIdx);

                    // Note: We do not add this rule to outerNestingCandidates,
                    // since it will probably still fail after nesting.
                    return false;

                } else {
                    Stats::add(Stats::SelfloopRanked);
                    TransIdx newIdx = its.addRule(std::move(newRule));

                    // In case we only got here in a second attempt (by some heuristic), clear some statistics
                    failedRulesNeedingEmptyLoop.erase(ruleIdx);
                    keepRules.erase(ruleIdx);

                    // Since acceleration worked, the resulting rule could be an inner loop for nesting
                    innerNestingCandidates.push_back({ ruleIdx, newIdx });

                    // We also try the original, unaccelerated rule as outer loop for nesting (as in the Unsat case)
                    outerNestingCandidates.push_back({ ruleIdx });

                    proofout << "Simple loop " << ruleIdx << " has the metering function ";
                    proofout << res.metering << ", resulting in the new transition ";
                    proofout << newIdx << "." << endl;
                    return true;
                }
            }
        default:;
    }
    unreachable();
}


bool Accelerator::accelerateAndStore(TransIdx ruleIdx, const LinearRule &rule, bool storeOnlySuccessful) {
    MeteringFinder::Result res = MeteringFinder::generate(its, rule);

    if (storeOnlySuccessful
        && res.result != MeteringFinder::Unbounded
        && res.result != MeteringFinder::Success) {
        return false;
    }

    return handleMeteringResult(ruleIdx, rule, res);
}


optional<LinearRule> Accelerator::accelerate(const LinearRule &rule) const {
    MeteringFinder::Result res = MeteringFinder::generate(its, rule);

    if (res.result == MeteringFinder::Unbounded) {
        Stats::add(Stats::SelfloopInfinite);
        debugAccel("Farkas (nested) unbounded for rule " << rule);

        // The rule is nonterminating. We can ignore the update, but the guard still has to be kept.
        LinearRule newRule = rule;
        newRule.getCostMut() = Expression::InfSymbol;
        newRule.getUpdateMut().clear();
        return newRule;

        // TODO: implement proof output here? (should only be called from nesting)
    }

    if (res.result == MeteringFinder::Success) {
        debugAccel("Farkas success, got " << res.metering << " for rule " << rule);

        // The metering function might need additional guards
        LinearRule newRule = rule;
        if (res.integralConstraint) {
            newRule.getGuardMut().push_back(res.integralConstraint.get());
        }

        if (Recurrence::calcIterated(its, newRule, res.metering)) {
            Stats::add(Stats::SelfloopRanked);
            return newRule;

            // TODO: implement proof output here? (should only be called from nesting)
        }
    }

    return {};
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
    // so it might be possible the execute the inner loop again (and thus nesting might work).
    for (const auto &it : outer.getUpdate()) {
        ExprSymbol updated = its.getGinacSymbol(it.first);
        if (innerGuardSyms.count(updated) > 0) {
            return true;
        }
    }

    return false;
}


void Accelerator::addNestedRule(const LinearRule &accelerated, const LinearRule &chain,
                                    TransIdx inner, TransIdx outer,
                                    vector<InnerNestingCandidate> &nested) {
    // Add the new rule
    TransIdx newTrans = its.addRule(accelerated);

    // Try to use the resulting rule as inner rule again later on
    // (in case there are actually 3 nested loops)
    nested.push_back({inner, newTrans});

    // The outer rule was accelerated (after nesting), so we do not need to keep it anymore
    keepRules.erase(outer);

    proofout << "Nested parallel self-loops " << outer << " (outer loop) and " << inner;
    proofout << " (inner loop), resulting in the new transitions: " << newTrans;

    // Try to combine chain and the accelerated loop
    auto chained = Chaining::chainRules(its, chain, accelerated);
    if (chained) {
        TransIdx chainedTrans = its.addRule(chained.get());
        nested.push_back({inner, chainedTrans});
        proofout << ", " << chainedTrans;
    }
    proofout << "." << endl;
}



bool Accelerator::nestRules(const InnerNestingCandidate &inner, const OuterNestingCandidate &outer,
                            vector<InnerNestingCandidate> &nested) {
    bool res = false;
    const LinearRule &innerRule = its.getRule(inner.newRule);
    const LinearRule &outerRule = its.getRule(outer.oldRule);

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

    // Try to nest, executing inner loop first
    auto innerFirst = Chaining::chainRules(its, innerRule, outerRule);
    if (innerFirst) {
        auto accelerated = accelerate(innerFirst.get());
        if (accelerated) {
            LinearRule newRule = accelerated.get();

            if (newRule.getCost().getComplexity() >= innerRule.getCost().getComplexity()) {
                res = true;

                // Add the accelerated rule.
                // Also try to first execute outer once before the accelerated rule.
                // FIXME: Is this ("outer once before") really effective? Check on benchmark set! Appears to be quite ugly
                addNestedRule(newRule, outerRule, inner.oldRule, outer.oldRule, nested);
            }
        }
    }

    // Try to nest, executing outer loop first
    auto outerFirst = Chaining::chainRules(its, outerRule, innerRule);
    if (outerFirst) {
        auto accelerated = accelerate(outerFirst.get());
        if (accelerated) {
            LinearRule newRule = accelerated.get();

            if (newRule.getCost().getComplexity() >= innerRule.getCost().getComplexity()) {
                res = true;

                // Add the accelerated rule.
                // Also try to first execute inner once before the accelerated rule.
                // FIXME: Is this ("inner once before") really effective? Check on benchmark set! Appears to be quite ugly
                addNestedRule(newRule, innerRule, inner.oldRule, outer.oldRule, nested);
            }
        }
    }

    return res;
}



// #####################
// ## Main algorithm  ##
// #####################


void Accelerator::run() {
    // Since we might add accelerated loops, we store the list of loops before acceleration
    vector<TransIdx> loops = its.getTransitionsFromTo(targetLoc, targetLoc);
    assert(!loops.empty());


    // Proof output
    proofout << "Accelerating the following rules:" << endl;
    for (TransIdx loop : loops) {
        LinearITSExport::printLabeledRule(loop, its, proofout);
    }


    // TODO: Might restructure this code to apply all heuristics directly for each rule (so we do not need rulesWith*)
    // TODO: This might simplify the decision of what to do with the rule (nest it? add empty rule? keep it?)

    // TODO: Maybe: Add method handleWithResult() and pass the original result (before the heuristics), but only if heuristics fail
    // TODO: Or add a methods handleFailed(), handleSuccessful() etc.

    // Try to accelerate all loops
    for (TransIdx loop : loops) {
        // don't try to accelerate loops with INF cost
        if (its.getRule(loop).getCost().isInfty()) {
            debugAccel("Keeping unaccelerated rule with infty cost: " << loop);
            keepRules.insert(loop);
            continue;
        }

        accelerateAndStore(loop, its.getRule(loop));

        if (Timeout::soft()) {
            goto timeout;
        }
    }


#ifdef FARKAS_HEURISTIC_INSTANTIATE_FREEVARS
    // Instantiate temporary variables by their bounds (might help to find a metering function)
    for (TransIdx loop : rulesWithUnsatMetering) {
        LinearRule rule = its.getRule(loop);
        debugAccel("Trying temp var instantiation for rule: " << rule);

        if (MeteringFinder::instantiateTempVarsHeuristic(its, rule)) {
            if (accelerateAndStore(loop, rule, true)) {
                debugAccel("Temp var instantiation successful with modified rule: " << rule);
            }
        }

        if (Timeout::soft()) {
            goto timeout;
        }
    }
#endif

#ifdef FARKAS_HEURISTIC_FOR_MINMAX
    // Min-Max heuristic (workaround for missing min/max(A,B) support)
    for (const ConflictVarsCandidate &can : rulesWithConflictingVariables) {
        VariableIdx A,B;
        tie(A,B) = can.conflictVars;
        LinearRule rule = its.getRule(can.oldRule);
        debugAccel("Trying MinMax heuristic with variables " << its.getVarName(A) << ", " << its.getVarName(B) << " for rule " << rule);

        // TODO: check satisfiability after adding constraints?

        // Add A > B to the guard, try to accelerate
        rule.getGuardMut().push_back(its.getGinacSymbol(A) > its.getGinacSymbol(B));
        if (accelerateAndStore(can.oldRule, rule, true)) {
            debugAccel("MinMax heuristic (A > B) successful with rule: " << rule);
        }

        // Add A < B to the guard, try to accelerate
        rule.getGuardMut().pop_back();
        rule.getGuardMut().push_back(its.getGinacSymbol(A) < its.getGinacSymbol(B));
        if (accelerateAndStore(can.oldRule, rule, true)) {
            debugAccel("MinMax heuristic (A < B) successful with rule: " << rule);
        }
    }
#endif

#ifdef FARKAS_TRY_ADDITIONAL_GUARD
    // Guard strengthening heuristic (might help to find a metering function)
    for (TransIdx loop : rulesWithUnsatMetering) {
        LinearRule rule = its.getRule(loop);
        debugAccel("Trying guard strengthening for rule: " << rule);

        if (FarkasMeterGenerator::prepareGuard(its, rule)) {
            if (accelerateAndStore(loop, rule, true)) {
                debugAccel("Guard strengthening successful with modified rule: " << rule);
            }
        }

        if (Timeout::soft()) {
            goto timeout;
        }
    }
#endif


    // Nesting
    for (int i=0; i < NESTING_MAX_ITERATIONS; ++i) {
        debugAccel("Nesting iteration: " << i);
        bool changed = false;
        vector<InnerNestingCandidate> newInnerCandidates;

        // Try to combine previously identified inner and outer candidates via chaining,
        // then try to accelerate the resulting rule
        for (const InnerNestingCandidate &inner : innerNestingCandidates) {
            for (const OuterNestingCandidate &outer : outerNestingCandidates) {
                if (nestRules(inner, outer, newInnerCandidates)) {
                    changed = true;
                }

                if (Timeout::soft()) {
                    goto timeout;
                }
            }
        }
        debugAccel("Nested " << newInnerCandidates.size() << " loops");

        if (!changed || Timeout::soft()) {
            break;
        }

        // For the next iteration, use the successfully nested loops as inner loops.
        // This caputes examples where 3 or more loops are nested.
        innerNestingCandidates.swap(newInnerCandidates);
    }


    // in case of a timeout, we perform no further acceleration, but still delete the old rules
    timeout:;

    // Remove old rules
    proofout << "Removing the self-loops:";
    for (TransIdx loop : loops) {
        if (keepRules.count(loop) == 0) {
            proofout << " " << loop;
            its.removeRule(loop);
        }
    }
    proofout << "." << endl;

    // Add a dummy rule to simulate the effect of not executing any loop
    if (!failedRulesNeedingEmptyLoop.empty()) {
        TransIdx t = its.addRule(LinearRule::dummyRule(targetLoc, targetLoc));
        proofout << "Adding an empty self-loop: " << t << "." << endl;
    }

    // FIXME: Remember to call this after accelerating loops!
    // removeDuplicateTransitions(getTransFromTo(node,node));
}



bool Accelerator::accelerateSimpleLoops(LinearITSProblem &its, LocationIdx loc) {
    if (its.getTransitionsFromTo(loc, loc).empty()) {
        return false;
    }

    proofout << endl;
    proofout.setLineStyle(ProofOutput::Headline);
    proofout << "Accelerating simple loops of location " << loc << "." << endl;
    proofout.increaseIndention();

    // Preprocessing
#ifdef CHAIN_BEFORE_ACCELERATE
    if (Accelerator::chainAllLoops(its, loc)) {
        proofout << "Chained all pairs of simple loops (where possible)" << endl;
    }
#endif

#ifdef SELFLOOPS_ALWAYS_SIMPLIFY
    bool simplifiedAny = false;
    for (TransIdx loop : its.getTransitionsFromTo(loc, loc)) {
        if (simplifyRule(its, its.getRuleMut(loop))) {
            simplifiedAny = true;
            debugAccel("Simplified rule " << loop << " to " << its.getRule(loop));
        }
    }
    if (simplifiedAny) {
        proofout << "Simplified some of the simple loops" << endl;
    }
#endif

    // Accelerate all loops (includes optimizations like nesting)
    Accelerator accel(its, loc);
    accel.run();

    proofout.decreaseIndention();
    return true;
}
