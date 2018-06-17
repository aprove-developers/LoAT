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

#include "analysis.h"

#include "expr/relation.h"
#include "z3/z3toolbox.h"
#include "asymptotic/asymptoticbound.h"

#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"

#include "prune.h"
#include "preprocess.h"
#include "chain.h"
#include "chainstrategy.h"
#include "accelerate/accelerator.h"

#include "its/export.h"

// TODO: Remove after testing
#include "accelerate/backwardacceleration.h"


using namespace std;



RuntimeResult Analysis::analyze(ITSProblem &its) {
    Analysis analysis(its);
    return analysis.run();
}


Analysis::Analysis(ITSProblem &its)
        : its(its) {}


// ##############################
// ## Main Analysis Algorithm  ##
// ##############################

RuntimeResult Analysis::run() {
    setupDotOutput();

    proofout.section("Pre-processing the ITS problem");
    proofout.headline("Initial linear ITS problem");
    printForProof("Initial");

    if (Config::Analysis::EnsureNonnegativeCosts && ensureNonnegativeCosts()) {
        proofout.headline("Added constraints to the guards to ensure costs are nonnegative:");
        printForProof("Costs >= 0");
    }

    if (ensureProperInitialLocation()) {
        proofout.headline("Added a fresh start location (such that it has no incoming rules):");
        printForProof("Fresh start");
    }

    RuntimeResult runtime; // defaults to unknown complexity
    string eliminatedLocation; // for proof output of eliminateALocation
    bool acceleratedOnce = false; // whether we did at least one acceleration step
    bool nonlinearProblem = !its.isLinear(); // whether the ITS is (still) nonlinear

    if (Config::Analysis::Preprocessing) {
        Timing::Scope timer(Timing::Preprocess);

        if (Pruning::removeLeafsAndUnreachable(its)) {
            proofout.headline("Removed unreachable and leaf rules:");
            printForProof("Removed unreachable");
        }

        if (removeUnsatRules()) {
            proofout.headline("Removed rules with unsatisfiable guard:");
            printForProof("Removed unsat");
        }

        if (preprocessRules()) {
            proofout.headline("Simplified all rules, resulting in:");
            printForProof("Simplify");
        }

        if (Timeout::preprocessing()) {
            debugWarn("Timeout for pre-processing exceeded!");
        }
    }

    // We cannot prove any lower bound for an empty ITS
    if (its.isEmpty()) {
        return runtime;
    }

    proofout.section("Simplification by acceleration and chaining");

    while (!isFullySimplified()) {

        // Repeat linear chaining and simple loop acceleration
        bool changed;
        do {
            changed = false;
            set<TransIdx> acceleratedRules;

            // Special handling of nonlinear rules
            if (nonlinearProblem && Pruning::removeSinkRhss(its)) {
                changed = true;
                proofout.headline("Removed locations with no outgoing rules from right-hand sides");
                printForProof("Removed sinks");
            }
            if (Timeout::soft()) break;

            if (accelerateSimpleLoops(acceleratedRules)) {
                changed = true;
                acceleratedOnce = true;
                proofout.headline("Accelerated all simple loops using metering functions (where possible):");
                printForProof("Accelerate simple loops");
            }
            if (Timeout::soft()) break;

            if (chainAcceleratedLoops(acceleratedRules)) {
                changed = true;
                proofout.headline("Chained accelerated rules (with incoming rules):");
                printForProof("Chain accelerated rules");
            }
            if (Timeout::soft()) break;

            if (Pruning::removeLeafsAndUnreachable(its)) {
                changed = true;
                proofout.headline("Removed unreachable locations (and leaf rules with constant cost):");
                printForProof("Remove unreachable");
            }
            if (Timeout::soft()) break;

            if (chainLinearPaths()) {
                changed = true;
                proofout.headline("Eliminated locations (on linear paths):");
                printForProof("Chain linear paths");
            }
            if (Timeout::soft()) break;

            // Check if the ITS is now linear (we accelerated all nonlinear rules)
            if (changed && nonlinearProblem) {
                nonlinearProblem = !its.isLinear();
                if (!nonlinearProblem) {
                    proofout.section("Obtained a tail recursive problem, continuing simplification");
                }
            }
        } while (changed);

        // Avoid wasting time on chaining/pruning if we are already done
        if (isFullySimplified()) {
            break;
        }

        // Try more involved chaining strategies if we no longer make progress
        if (chainTreePaths()) {
            proofout.headline("Eliminated locations (on tree-shaped paths):");
            printForProof("Chain tree paths");

        } else if (eliminateALocation(eliminatedLocation)) {
            proofout.headline("Eliminated location " + eliminatedLocation + " (as a last resort):");
            printForProof("Eliminate location");
        }
        if (Timeout::soft()) break;

        // Try to avoid rule explosion (often caused by chainTreePaths).
        // Since pruning relies on the rule's complexities, we only do this after the first acceleration.
        if (acceleratedOnce && pruneRules()) {
            proofout.headline("Applied pruning (of leafs and parallel rules):");
            printForProof("Prune");
        }
        if (Timeout::soft()) break;
    }

    if (Timeout::soft()) {
        proofout.warning("Aborted due to lack of remaining time");
    }

    if (isFullySimplified()) {
        // Remove duplicate rules (ignoring updates) to avoid wasting time on asymptotic bounds
        Pruning::removeDuplicateRules(its, its.getTransitionsFrom(its.getInitialLocation()), false);
    }

    if (Config::Output::ExportSimplified) {
        proofout.headline("Fully simplified program in input format:");
        ITSExport::printKoAT(its, proofout);
        proofout << endl;
    }

    proofout.section("Computing asymptotic complexity");
    proofout.headline("Fully simplified ITS problem");
    printForProof("Final");

    if (!isFullySimplified()) {
        // A timeout occurred before we managed to complete the analysis.
        // We try to quickly extract at least some complexity results.
        proofout.warning("This is only a partial result (probably due to a timeout).");
        proofout << "Trying to find the maximal complexity that has already been derived." << endl;

        // Reduce the number of rules to avoid z3 invocations
        removeConstantPathsAfterTimeout();
        proofout.headline("Removed rules with constant/unknown complexity:");
        printForProof("Removed constant");

        // Try to find a high complexity in the remaining problem (with chaining, but without acceleration)
        runtime = getMaxPartialResult();

    } else {
        // No timeout, fully simplified, find the maximum runtime
        runtime = getMaxRuntime();
    }

    // if we failed to prove a bound, we can still output O(1) with bound 1, as the graph was non-empty
    if (runtime.cpx == Complexity::Unknown) {
        runtime.cpx = Complexity::Const;
        runtime.bound = Expression(1);
        runtime.guard.clear();
    }

    finalizeDotOutput(runtime);

    return runtime;
}


// ############################
// ## Preprocessing, Output  ##
// ############################

bool Analysis::ensureProperInitialLocation() {
    if (its.hasTransitionsTo(its.getInitialLocation())) {
        LocationIdx newStart = its.addLocation();
        its.addRule(Rule::dummyRule(newStart, its.getInitialLocation()));
        its.setInitialLocation(newStart);
        return true;
    }
    return false;
}


bool Analysis::ensureNonnegativeCosts() {
    bool changed = false;

    for (TransIdx trans : its.getAllTransitions()) {
        Rule &rule = its.getRuleMut(trans);

        // Add the constraint unless it is trivial (e.g. if the cost is 1).
        Expression costConstraint = rule.getCost() >= 0;
        if (Relation::isTriviallyTrue(costConstraint)) continue;

        rule.getGuardMut().push_back(costConstraint);
        changed = true;
    }
    return changed;
}


bool Analysis::removeUnsatRules() {
    bool changed = false;

    for (TransIdx rule : its.getAllTransitions()) {
        if (Timeout::preprocessing()) break;

        if (Z3Toolbox::checkAll(its.getRule(rule).getGuard()) == z3::unsat) {
            its.removeRule(rule);
            changed = true;
        }
    }

    return changed;
}


bool Analysis::preprocessRules() {
    bool changed = false;

    // update/guard preprocessing
    for (TransIdx idx : its.getAllTransitions()) {
        if (Timeout::preprocessing()) return changed;

        Rule &rule = its.getRuleMut(idx);
        changed = Preprocess::preprocessRule(its, rule) || changed;
    }

    // remove duplicates
    for (LocationIdx node : its.getLocations()) {
        for (LocationIdx succ : its.getSuccessorLocations(node)) {
            if (Timeout::preprocessing()) return changed;

            changed = Pruning::removeDuplicateRules(its, its.getTransitionsFromTo(node, succ)) || changed;
        }
    }

    return changed;
}


bool Analysis::isFullySimplified() const {
    for (LocationIdx node : its.getLocations()) {
        if (its.isInitialLocation(node)) continue;
        if (its.hasTransitionsFrom(node)) return false;
    }
    return true;
}


void Analysis::printForProof(const std::string &dotDescription) {
    // Proof output
    proofout.increaseIndention();
    ITSExport::printForProof(its, proofout);
    proofout.decreaseIndention();

    if (dotStream.is_open()) {
        LinearITSExport::printDotSubgraph(its, dotCounter++, dotDescription, dotStream);
    }
}


void Analysis::setupDotOutput() {
    if (!Config::Output::DotFile) {
        return;
    }

    if (!its.isLinear()) {
        bool proofWasEnabled = proofout.setEnabled(true);
        proofout.warning("Dot output is only applicable to non-recursive ITS problems, disabling.");
        proofout.setEnabled(proofWasEnabled);
        return;
    }

    string file = Config::Output::DotFile.get();
    debugLinear("Trying to open dot output file: " << file);
    dotStream.open(file);

    if (!dotStream.is_open()) {
        bool proofWasEnabled = proofout.setEnabled(true);
        proofout.warning("Could not open file " + file + " for dot output, disabling.");
        proofout.setEnabled(proofWasEnabled);
        return;
    }

    dotStream << "digraph {" << endl;
}


void Analysis::finalizeDotOutput(const RuntimeResult &runtime) {
    if (dotStream.is_open()) {
        LinearITSExport::printDotText(++dotCounter, runtime.cpx.toString(), dotStream);
        dotStream << "}" << endl;
        dotStream.close();
    }
}


// ##############################
// ## Acceleration & Chaining  ##
// ##############################

bool Analysis::chainLinearPaths() {
    Stats::addStep("Linear::chainLinearPaths");
    return Chaining::chainLinearPaths(its);
}


bool Analysis::chainTreePaths() {
    Stats::addStep("Linear::chainTreePaths");
    return Chaining::chainTreePaths(its);
}


bool Analysis::eliminateALocation(string &eliminatedLocation) {
    Stats::addStep("Linear::eliminateALocation");
    return Chaining::eliminateALocation(its, eliminatedLocation);
}


bool Analysis::chainAcceleratedLoops(const set<TransIdx> &acceleratedRules) {
    Stats::addStep("Linear::chainAcceleratedLoops");
    return Chaining::chainAcceleratedRules(its, acceleratedRules);
}


bool Analysis::accelerateSimpleLoops(set<TransIdx> &acceleratedRules) {
    Stats::addStep("FlowGraph::accelerateSimpleLoops");
    bool res = false;

    for (LocationIdx node : its.getLocations()) {
        res = Accelerator::accelerateSimpleLoops(its, node, acceleratedRules) || res;
        if (Timeout::soft()) return res;
    }

    return res;
}

/*
bool Analysis::backwardAccelerateSimpleLoops() {
    Stats::addStep("FlowGraph::backwardAccelerateSimpleLoops");
    bool res = false;

    for (LocationIdx node : its.getLocations()) {
        vector<TransIdx> loops = its.getTransitionsFromTo(node, node);
        vector<TransIdx> toRemove;
        vector<LinearRule> toAdd;

        debugBackwardAccel("trying to backward-accelerate " << loops.size() << " loops at location " << node);
        for (TransIdx loop : loops) {
            if (Timeout::soft()) break;

            auto optRules = BackwardAcceleration::accelerate(its, its.getLinearRule(loop));
            if (optRules) {
                vector<LinearRule> rules = optRules.get();
                toAdd.insert(toAdd.end(), rules.begin(), rules.end());
                toRemove.push_back(loop);
            }
        }

        for (TransIdx rule : toRemove) {
            its.removeRule(rule);
        }
        for (LinearRule rule : toAdd) {
            its.addRule(rule);
        }
        res = res || !toRemove.empty();
    }
    return res;
}
*/

bool Analysis::pruneRules() {
    // Always remove unreachable rules
    bool changed = Pruning::removeLeafsAndUnreachable(its);

    // Prune parallel transitions if enabled
    if (Config::Analysis::Pruning) {
        Stats::addStep("Linear::pruneRules");
        changed = Pruning::pruneParallelRules(its) || changed;
    }

    return changed;
}


// #############################
// ## Complexity Computation  ##
// #############################

/**
 * Helper for getMaxRuntime that searches for the maximal cost.getComplexity().
 * Note that this does not involve the asymptotic bounds check and thus not give sound results!
 */
static RuntimeResult getMaxComplexity(const ITSProblem &its, set<TransIdx> rules) {
    RuntimeResult res;

    for (TransIdx rule : rules) {
        Complexity cpxRule = its.getRule(rule).getCost().getComplexity();
        if (cpxRule > res.cpx) {
            res.cpx = cpxRule;
            res.guard = its.getRule(rule).getGuard();
            res.bound = its.getRule(rule).getCost();
        }
    }

    return res;
}


RuntimeResult Analysis::getMaxRuntime() {
    auto rules = its.getTransitionsFrom(its.getInitialLocation());
    auto isTempVar = [&](const ExprSymbol &var){ return its.isTempVar(var); };

    if (!Config::Analysis::AsymptoticCheck) {
        proofout.warning("WARNING: The asymptotic check is disabled, the result might be unsound!");
        return getMaxComplexity(its, rules);
    }

    RuntimeResult res;
    for (TransIdx ruleIdx : rules) {
        Rule &rule = its.getRuleMut(ruleIdx);
        const Expression &cost = rule.getCost();

        // getComplexity() is not sound, but gives an upperbound, so we can avoid useless asymptotic checks.
        // We have to be careful with temp variables, since they can lead to unbounded cost.
        Complexity cpxUpperbound = cost.getComplexity();
        bool hasTempVar = !cost.isInfSymbol() && cost.hasVariableWith(isTempVar);
        if (cpxUpperbound <= res.cpx && !hasTempVar) {
            proofout << "Skipping rule " << ruleIdx << " since it cannot improve the complexity" << endl;
            continue;
        }

        proofout << endl;
        proofout.setLineStyle(ProofOutput::Headline);
        proofout << "Computing asymptotic complexity for rule " << ruleIdx << endl;
        proofout.increaseIndention();

        // Simplify guard to speed up asymptotic check
        bool simplified = false;
        simplified |= Preprocess::simplifyGuard(rule.getGuardMut());
        simplified |= Preprocess::simplifyGuardBySmt(rule.getGuardMut());
        if (simplified) {
            proofout << "Simplified the guard:" << endl;
            ITSExport::printLabeledRule(ruleIdx, its, proofout);
        }
        if (Timeout::hard()) break;

        // Perform the asymptotic check to verify that this rule's guard allows infinitely many models
        auto checkRes = AsymptoticBound::determineComplexity(its, rule.getGuard(), cost, true);

        debugLinear("Asymptotic result: " << checkRes.cpx << " because: " << checkRes.reason);
        proofout << "Resulting cost " << checkRes.cost << " has complexity: " << checkRes.cpx << endl;
        proofout.decreaseIndention();

        if (checkRes.cpx > res.cpx) {
            proofout << endl;
            proofout.setLineStyle(ProofOutput::Result);
            proofout << "Found new complexity " << checkRes.cpx << ", because: " << checkRes.reason << "." << endl;

            res.cpx = checkRes.cpx;
            res.bound = checkRes.cost;
            res.reducedCpx = checkRes.reducedCpx;
            res.guard = rule.getGuard();

            if (res.cpx >= Complexity::Infty) {
                break;
            }
        }

        proofout << endl;
        if (Timeout::hard()) break;
    }

#ifdef DEBUG_PROBLEMS
    // Check if we lost complexity due to asymptotic bounds check (compared to getComplexity())
    // This may be fine, but it could also indicate a weakness in the asymptotic check.
    RuntimeResult unsoundRes = getMaxComplexity(its, rules);
    if (unsoundRes.cpx > res.cpx) {
        debugProblem("Asymptotic bounds lost complexity: " << unsoundRes.cpx << " [" << unsoundRes.bound << "]"
                << "--> " << res.cpx << " [" << res.bound << "]");
    }
#endif

    return res;
}


// ###############################
// ## Complexity After Timeout  ##
// ###############################

/**
 * Helper for removeConstantRulesAfterTimeout.
 * Returns true if there are no non-constant rules reachable from curr.
 */
static bool removeConstantPathsImpl(ITSProblem &its, LocationIdx curr, set<LocationIdx> &visited) {
    if (visited.insert(curr).second == false) return true; //already seen, remove any transitions forming a loop

    for (LocationIdx next : its.getSuccessorLocations(curr)) {
        if (Timeout::hard()) return false;

        // Check if all rules reachable from next have constant cost.
        // In this case, all constant rules leading to next are not interesting and can be removed.
        if (removeConstantPathsImpl(its, next, visited)) {
            for (TransIdx rule : its.getTransitionsFromTo(curr, next)) {
                if (its.getRule(rule).getCost().getComplexity() <= Complexity::Const) {
                    its.removeRule(rule);
                }
            }
        }
    }

    // If all rules have been deleted, no non-constant rules are reachable and curr is not of any interest.
    return its.getTransitionsFrom(curr).empty();
}


// TODO: generalize this to remove all rules below a certain complexity
// TODO: this can then be called again whenever a new complexity has been derived
// TODO: This should help quite a bit, since we can get rid of expensive chaining costs
void Analysis::removeConstantPathsAfterTimeout() {
    set<LocationIdx> visited;
    removeConstantPathsImpl(its, its.getInitialLocation(), visited);
}


RuntimeResult Analysis::getMaxPartialResult() {
    //contract and always compute the maximum complexity to allow abortion at any time
    RuntimeResult res;
    LocationIdx initial = its.getInitialLocation(); // just a shorthand
    auto isTempVar = [&](const ExprSymbol &var){ return its.isTempVar(var); }; // just a shorthand

    while (true) {
        //always check for timeouts
        if (Timeout::hard()) goto abort;

        //get current max cost (with asymptotic bounds check)
        for (TransIdx trans : its.getTransitionsFrom(initial)) {
            Rule &rule = its.getRuleMut(trans);

            // check if we can skip this rule
            const Expression &cost = rule.getCost();
            bool hasTempVar = !cost.isInfSymbol() && cost.hasVariableWith(isTempVar);
            if (cost.getComplexity() <= max(res.cpx, Complexity::Const) && !hasTempVar) continue;

            proofout << endl;
            proofout.setLineStyle(ProofOutput::Headline);
            proofout << "Computing asymptotic complexity for rule " << trans << endl;
            proofout.increaseIndention();

            // Simplify guard to speed up asymptotic check
            bool simplified = false;
            simplified |= Preprocess::simplifyGuard(rule.getGuardMut());
            simplified |= Preprocess::simplifyGuardBySmt(rule.getGuardMut());
            if (simplified) {
                proofout << "Simplified the guard:" << endl;
                ITSExport::printLabeledRule(trans, its, proofout);
            }
            if (Timeout::hard()) goto abort;

            // Perform the asymptotic check to verify that this rule's guard allows infinitely many models
            auto checkRes = AsymptoticBound::determineComplexity(its, rule.getGuard(), rule.getCost(), true);

            proofout.decreaseIndention();

            if (checkRes.cpx > res.cpx) {
                proofout << endl;
                proofout.setLineStyle(ProofOutput::Result);
                proofout << "Found new complexity " << checkRes.cpx << ", because: " << checkRes.reason << "." << endl;

                res.cpx = checkRes.cpx;
                res.bound = checkRes.cost;
                res.reducedCpx = checkRes.reducedCpx;
                res.guard = rule.getGuard();

                if (res.cpx >= Complexity::Infty) goto done;
            }
            if (Timeout::hard()) goto abort;
        }

        //contract next level (if there is one)
        auto succs = its.getSuccessorLocations(initial);
        if (succs.empty()) goto done;

        for (LocationIdx succ : succs) {
            for (TransIdx first : its.getTransitionsFromTo(initial,succ)) {
                for (TransIdx second : its.getTransitionsFrom(succ)) {

                    auto chained = Chaining::chainRules(its, its.getRule(first), its.getRule(second));
                    if (chained) {
                        its.addRule(chained.get());
                    }

                    if (Timeout::hard()) goto abort;
                }

                // We already computed the complexity above, and tried to change it just now, that's enough.
                its.removeRule(first);
            }
        }
        proofout.headline("Performed chaining from the start location:");
        printForProof("Chaining from start");
    }

abort:
    proofout << "Aborting due to timeout" << endl;
done:
    return res;
}

