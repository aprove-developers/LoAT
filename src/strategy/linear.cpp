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

#include "linear.h"

#include "z3/z3toolbox.h"
#include "asymptotic/asymptoticbound.h"

#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"

#include "prune.h"
#include "chaining.h"
#include "accelerate.h"

#include "preprocess.h"
#include "its/export.h"


using namespace std;



RuntimeResult LinearITSAnalysis::analyze(LinearITSProblem &its, AnalysisSettings cfg) {
    LinearITSAnalysis analysis(its, cfg);
    return analysis.run();
}


LinearITSAnalysis::LinearITSAnalysis(LinearITSProblem &its, AnalysisSettings cfg)
        : its(its), cfg(cfg) {
}


RuntimeResult LinearITSAnalysis::run() {
    if (cfg.dotOutput) {
        cfg.dotStream << "digraph {" << endl;
    }

    proofout.section("Pre-processing the ITS problem");
    proofout.headline("Initial linear ITS problem");
    printForProof("Initial");

    // TODO: Add the "cost >= 0" terms here, this is not something the parser should do!
    // TODO: But only add if it is not already implied (this is much better than the hacky removeal of the last guard element)

    if (ensureProperInitialLocation()) {
        proofout.headline("Added a fresh start location (such that it has no incoming rules):");
        printForProof("Fresh start");
    }

    if (Pruning::removeUnsatInitialRules(its)) {
        proofout.headline("Removed unsatisfiable initial rules:");
        printForProof("Reduced initial");
    }

    RuntimeResult runtime; // defaults to unknown complexity

    // We cannot prove any lower bound for an empty ITS
    if (its.isEmpty()) {
        return runtime;
    }

    if (cfg.doPreprocessing) {
        if (preprocessRules()) {
            proofout.headline("Simplified all rules, resulting in:");
            printForProof("Simplify");
        }
    }

    proofout.section("Simplification by acceleration and chaining");

    while (!isFullySimplified()) {

        // Repeat linear chaining and simple loop acceleration
        bool changed;
        do {
            changed = false;

            if (accelerateSimpleLoops()) {
                changed = true;
                proofout.headline("Accelerated all simple loops using metering functions (where possible):");
                printForProof("Accelerate simple loops");
            }
            if (Timeout::soft()) break;

            if (chainSimpleLoops()) {
                changed = true;
                proofout.headline("Chained simple loops (with incoming rules):");
                printForProof("Chain simple loops");
            }
            if (Timeout::soft()) break;

            if (chainLinearPaths()) {
                changed = true;
                proofout.headline("Eliminated locations (on linear paths):");
                printForProof("Chain linear paths");
            }
            if (Timeout::soft()) break;

        } while (changed);

        // Avoid wasting time on chaining/pruning if we are already done
        if (isFullySimplified()) {
            break;
        }

        // Try more involved chaining strategies if we no longer make progress
        if (chainTreePaths()) {
            proofout.headline("Eliminated locations (on tree-shaped paths):");
            printForProof("Chain tree paths");

        } else if (eliminateALocation()) {
            proofout.headline("Eliminated a location (as a last resort):");
            printForProof("Eliminate location");
        }
        if (Timeout::soft()) break;

        // Try to avoid rule explosion
        if (pruneRules()) {
            proofout << endl <<  "Applied pruning (of leafs and parallel rules):" << endl;
            printForProof("Prune");
        }
        if (Timeout::soft()) break;
    }

    if (Timeout::soft()) {
        proofout << endl;
        proofout.setLineStyle(ProofOutput::Warning);
        proofout << "Aborted due to lack of remaining time" << endl << endl;
    }

    if (isFullySimplified()) {
        // Remove duplicate rules (ignoring updates) to avoid wasting time on asymptotic bounds
        Pruning::removeDuplicateRules(its, its.getTransitionsFrom(its.getInitialLocation()), false);
    }

    if (cfg.printSimplifiedAsKoAT) {
        proofout.headline("Fully simplified program in input format:");
        LinearITSExport::printKoAT(its, proofout);
        proofout << endl;
    }

    proofout.section("Computing asymptotic complexity");
    proofout.headline("Fully simplified ITS problem");
    printForProof("Final");

    if (!isFullySimplified()) {
        // A timeout occurred before we managed to complete the analysis.
        // We try to quickly extract at least some complexity results.
        proofout.setLineStyle(ProofOutput::Warning);
        proofout << "This is only a partial result (probably due to a timeout)." << endl;
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

    if (cfg.dotOutput) {
        LinearITSExport::printDotText(++dotCounter, runtime.cpx.toString(), cfg.dotStream);
        cfg.dotStream << "}" << endl;
    }

    return runtime;
}



bool LinearITSAnalysis::ensureProperInitialLocation() {
    if (!its.getPredecessorLocations(its.getInitialLocation()).empty()) {
        LocationIdx newStart = its.addLocation();
        its.addRule(LinearRule::dummyRule(newStart, its.getInitialLocation()));
        its.setInitialLocation(newStart);
        return true;
    }
    return false;
}


bool LinearITSAnalysis::preprocessRules() {
    Timing::Scope _timer(Timing::Preprocess);

    // remove unreachable transitions/nodes
    bool changed = Pruning::removeLeafsAndUnreachable(its);

    // update/guard preprocessing
    for (LocationIdx node : its.getLocations()) {
        for (TransIdx idx : its.getTransitionsFrom(node)) {
            if (Timeout::preprocessing()) return changed;

            LinearRule &rule = its.getRuleMut(idx);
            if (cfg.eliminateCostConstraints) {
                changed = Preprocess::tryToRemoveCost(rule.getGuardMut()) || changed;
            }
            changed = Preprocess::simplifyRule(its, rule) || changed;
        }
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


bool LinearITSAnalysis::isFullySimplified() const {
    for (LocationIdx node : its.getLocations()) {
        if (its.isInitialLocation(node)) continue;
        if (!its.getTransitionsFrom(node).empty()) return false;
    }
    return true;
}


bool LinearITSAnalysis::chainLinearPaths() {
    Stats::addStep("Linear::chainLinearPaths");
    bool res = Chaining::chainLinearPaths(its);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER CHAIN LINEAR ===========\\ " << endl;
    its.print(cout);
    cout << " \\========== AFTER CHAIN LINEAR ===========/ " << endl;
#endif
    return res;
}


bool LinearITSAnalysis::chainTreePaths() {
    Stats::addStep("Linear::chainTreePaths");
    bool res = Chaining::chainTreePaths(its);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER CHAIN TREE ===========\\ " << endl;
    its.print(cout);
    cout << " \\========== AFTER CHAIN TREE ===========/ " << endl;
#endif
    return res;
}


bool LinearITSAnalysis::eliminateALocation() {
    Stats::addStep("Linear::eliminateALocation");
    bool res = Chaining::eliminateALocation(its);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER ELIMINATING LOCATIONS ===========\\ " << endl;
    its.print(cout);
    cout << " \\========== AFTER ELIMINATING LOCATIONS ===========/ " << endl;
#endif
    return res;
}


bool LinearITSAnalysis::chainSimpleLoops() {
    Stats::addStep("FlowGraph::chainSimpleLoops");
    bool res = Chaining::chainSimpleLoops(its);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER CHAINING SIMPLE LOOPS ===========\\ " << endl;
    its.print(cout);
    cout << " \\========== AFTER CHAINING SIMPLE LOOPS ===========/ " << endl;
#endif
    return res;
}


bool LinearITSAnalysis::accelerateSimpleLoops() {
    Stats::addStep("FlowGraph::accelerateSimpleLoops");
    bool res = false;

    for (LocationIdx node : its.getLocations()) {
        res = Accelerator::accelerateSimpleLoops(its, node) || res;
        if (Timeout::soft()) return res;
    }

#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER SELFLOOPS ==========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER SELFLOOPS ==========/ " << endl;
#endif
    return res;
}


bool LinearITSAnalysis::pruneRules() {
    // Always remove unreachable rules
    bool changed = Pruning::removeLeafsAndUnreachable(its);

    // Prune parallel transitions if enabled
#ifdef PRUNING_ENABLE
    Stats::addStep("Linear::pruneRules");
    changed = Pruning::pruneParallelRules(its) || changed;
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER PRUNING ==========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER PRUNING ==========/ " << endl;
#endif
#endif

    return changed;
}


/* ### Final complexity calcuation ### */

/**
 * Helper for getMaxRuntime that searches for the maximal cost.getComplexity().
 * Note that this does not involve the asymptotic bounds check and thus not give sound results!
 */
static RuntimeResult getMaxComplexity(const LinearITSProblem &its, vector<TransIdx> rules) {
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


RuntimeResult LinearITSAnalysis::getMaxRuntime() {
    auto rules = its.getTransitionsFrom(its.getInitialLocation());

#ifndef FINAL_INFINITY_CHECK
    proofout.setLineStyle(ProofOutput::Warning);
    proofout << "WARNING: The asymptotic check is disabled, the result might be unsound!" << endl << endl;
    return getMaxComplexity(its, rules);
#endif

    RuntimeResult res;
    for (TransIdx ruleIdx : rules) {
        const LinearRule &rule = its.getRule(ruleIdx);

        // getComplexity() is not sound, but gives an upperbound, so we can avoid useless asymptotic checks
        Complexity cpxUpperbound = rule.getCost().getComplexity();
        if (cpxUpperbound <= res.cpx) {
            proofout << "Skipping rule " << ruleIdx << " since it cannot improve the complexity" << endl;
            continue;
        }

        proofout << endl;
        proofout.setLineStyle(ProofOutput::Headline);
        proofout << "Computing asymptotic complexity for rule " << ruleIdx << endl;
        proofout.increaseIndention();

        // Perform the asymptotic check to verify that this rule's guard allows infinitely many models
        auto checkRes = AsymptoticBound::determineComplexity(its, rule.getGuard(), rule.getCost(), true);

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


/* ### Recovering after timeout ### */

/**
 * Helper for removeConstantRulesAfterTimeout.
 * Returns true if there are no non-constant rules reachable from curr.
 */
static bool removeConstantPathsImpl(LinearITSProblem &its, LocationIdx curr, set<LocationIdx> &visited) {
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


void LinearITSAnalysis::removeConstantPathsAfterTimeout() {
    set<LocationIdx> visited;
    removeConstantPathsImpl(its, its.getInitialLocation(), visited);
}


RuntimeResult LinearITSAnalysis::getMaxPartialResult() {
    //contract and always compute the maximum complexity to allow abortion at any time
    RuntimeResult res;
    LocationIdx initial = its.getInitialLocation(); // just a shorthand

    while (true) {
        //always check for timeouts
        if (Timeout::hard()) goto abort;

        //get current max cost (with asymptotic bounds check)
        for (TransIdx trans : its.getTransitionsFrom(initial)) {
            const LinearRule &rule = its.getRule(trans);
            if (rule.getCost().getComplexity() <= max(res.cpx, Complexity::Const)) continue;

            proofout << endl;
            proofout.setLineStyle(ProofOutput::Headline);
            proofout << "Computing asymptotic complexity for rule " << trans << endl;
            proofout.increaseIndention();

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


void LinearITSAnalysis::printForProof(const std::string &dotDescription) {
    // Proof output
    proofout.increaseIndention();
    LinearITSExport::printForProof(its, proofout);
    proofout.decreaseIndention();

    // dot output
    LinearITSExport::printDotSubgraph(its, dotCounter++, dotDescription, cfg.dotStream);
}
