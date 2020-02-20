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

#include "analysis.hpp"

#include "../expr/relation.hpp"
#include "../z3/z3toolbox.hpp"
#include "../asymptotic/asymptoticbound.hpp"

#include "../util/timeout.hpp"

#include "prune.hpp"
#include "preprocess.hpp"
#include "chain.hpp"
#include "chainstrategy.hpp"
#include "../accelerate/accelerator.hpp"

#include "../its/export.hpp"

#include "../merging/rulemerger.hpp"
#include "../util/proofutil.hpp"


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

    proofout.headline("Initial ITS");
    printForProof("Initial");

    proofout.headline("Preprocessing");

    if (Config::Analysis::EnsureNonnegativeCosts && ensureNonnegativeCosts()) {
        proofout.section("Ensure Cost >= 0");
        printForProof("Costs >= 0");
    }

    if (ensureProperInitialLocation()) {
        proofout.section("Added a fresh start location without incoming rules");
        printForProof("Fresh start");
    }

    RuntimeResult runtime; // defaults to unknown complexity
    string eliminatedLocation; // for proof output of eliminateALocation
    bool acceleratedOnce = false; // whether we did at least one acceleration step
    bool nonlinearProblem = !its.isLinear(); // whether the ITS is (still) nonlinear

    // Check if we have at least constant complexity (i.e., at least one rule can be taken with cost >= 1)
    if (Config::Analysis::ConstantCpxCheck) {
        auto optRuntime = checkConstantComplexity();
        if (optRuntime) {
            runtime = optRuntime.get();
        }
    }

    if (Config::Analysis::Preprocessing) {

        if (Pruning::removeLeafsAndUnreachable(its)) {
            proofout.section("Removed unreachable rules and leafs");
            printForProof("Removed unreachable");
        }

        if (removeUnsatRules()) {
            proofout.section("Removed rules with unsatisfiable guard");
            printForProof("Removed unsat");
        }

        if (Pruning::removeLeafsAndUnreachable(its)) {
            proofout.section("Removed unreachable rules and leafs");
            printForProof("Removed unreachable");
        }

        if (preprocessRules()) {
            proofout.section("Simplified rules");
            printForProof("Simplify");
        }

    }

    // We cannot prove any lower bound for an empty ITS
    if (its.isEmpty()) {
        goto done;
    }

    proofout.headline("Finished Preprocessing");

    while (!isFullySimplified()) {

        // Repeat linear chaining and simple loop acceleration
        bool changed;
        do {
            changed = false;
            set<TransIdx> acceleratedRules;

            // Special handling of nonlinear rules
            if (nonlinearProblem && Pruning::removeSinkRhss(its)) {
                changed = true;
                proofout.headline("Removed sinks");
                printForProof("Removed sinks");
            }
            if (Timeout::soft()) break;

            if (accelerateSimpleLoops(acceleratedRules)) {
                changed = true;
                acceleratedOnce = true;
                proofout.headline("Accelerated simple loops");
                printForProof("Accelerate simple loops");
            }
            if (Timeout::soft()) break;

            if (chainAcceleratedLoops(acceleratedRules)) {
                changed = true;
                proofout.headline("Chained accelerated rules with incoming rules");
                printForProof("Chain accelerated rules");
            }
            if (Timeout::soft()) break;

            if (Pruning::removeLeafsAndUnreachable(its)) {
                changed = true;
                proofout.headline("Removed unreachable locations and irrelevant leafs");
                printForProof("Remove unreachable");
            }
            if (Timeout::soft()) break;

            if (chainLinearPaths()) {
                changed = true;
                proofout.headline("Eliminated locations on linear paths");
                printForProof("Chain linear paths");
            }
            if (Timeout::soft()) break;

            // Check if the ITS is now linear (we accelerated all nonlinear rules)
            if (changed && nonlinearProblem) {
                nonlinearProblem = !its.isLinear();
            }
        } while (changed);

        // Avoid wasting time on chaining/pruning if we are already done
        if (isFullySimplified()) {
            break;
        }

        // Try more involved chaining strategies if we no longer make progress
        if (chainTreePaths()) {
            proofout.headline("Eliminated locations on tree-shaped paths");
            printForProof("Chain tree paths");

        } else if (eliminateALocation(eliminatedLocation)) {
            proofout.headline("Eliminated location " + eliminatedLocation);
            printForProof("Eliminate location");
        }
        if (Timeout::soft() || isFullySimplified()) break;

        if (acceleratedOnce) {

            if (merging::RuleMerger::mergeRules(its)) {
                proofout.headline("Merged rules");
                printForProof("Merging");
            }

            // Try to avoid rule explosion (often caused by chainTreePaths).
            // Since pruning relies on the rule's complexities, we only do this after the first acceleration.
            if (pruneRules()) {
                proofout.headline("Applied pruning (of leafs and parallel rules):");
                printForProof("Prune");
            }
        }

        if (Timeout::soft()) break;
    }

    if (Timeout::soft()) {
        std::cerr << "Aborted due to timeout" << std::endl;
    }

    if (isFullySimplified()) {
        // Remove duplicate rules (ignoring updates) to avoid wasting time on asymptotic bounds
        if (Pruning::removeDuplicateRules(its, its.getTransitionsFrom(its.getInitialLocation()), false)) {
            proofout.headline("Removed duplicate rules (ignoring updates)");
            printForProof("Remove duplicates");
        }
    }

    if (Config::Output::ExportSimplified) {
        cout << "Fully simplified program in input format:" << endl;
        ITSExport::printKoAT(its, cout);
    }

    proofout.section("Computing asymptotic complexity");

    if (!isFullySimplified()) {
        // A timeout occurred before we managed to complete the analysis.
        // We try to quickly extract at least some complexity results.
        proofout.warning("This is only a partial result (probably due to a timeout).");
        proofout.append("Trying to find the maximal complexity that has already been derived.");

        // Reduce the number of rules to avoid z3 invocations
        removeConstantPathsAfterTimeout();

        // Try to find a high complexity in the remaining problem (with chaining, but without acceleration)
        RuntimeResult res = getMaxPartialResult();
        if (res.cpx != Complexity::Unknown) {
            runtime = res;
        }

    } else {
        // No timeout, fully simplified, find the maximum runtime
        RuntimeResult res = getMaxRuntime();
        if (res.cpx != Complexity::Unknown) {
            runtime = res;
        }
    }

done:
    printResult(runtime);
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
    std::stringstream s;
    ITSExport::printForProof(its, s);
    proofout.append(s);

    if (dotStream.is_open()) {
        LinearITSExport::printDotSubgraph(its, dotCounter++, dotDescription, dotStream);
    }
}


void Analysis::printResult(const RuntimeResult &runtime) {
    proofout.newline();
    proofout.result("Proved the following lower bound");

    proofout.result(stringstream() << "Complexity:  " << runtime.cpx);
    stringstream s;
    s << "Cpx degree: ";
    switch (runtime.cpx.getType()) {
        case Complexity::CpxPolynomial: s << runtime.cpx.getPolynomialDegree().toFloat() << endl; break;
        case Complexity::CpxUnknown: s << "?" << endl; break;
        default: s << runtime.cpx << endl;
    }
    s << endl;
    s << "Solved cost: " << runtime.solvedCost << endl;
    s << "Rule cost:   ";
    ITSExport::printCost(runtime.cost, s);
    s << endl;
    s << "Rule guard:  ";
    ITSExport::printGuard(runtime.guard, s);
    proofout.result(s);
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
    return Chaining::chainLinearPaths(its);
}


bool Analysis::chainTreePaths() {
    return Chaining::chainTreePaths(its);
}


bool Analysis::eliminateALocation(string &eliminatedLocation) {
    return Chaining::eliminateALocation(its, eliminatedLocation);
}


bool Analysis::chainAcceleratedLoops(const set<TransIdx> &acceleratedRules) {
    return Chaining::chainAcceleratedRules(its, acceleratedRules);
}


bool Analysis::accelerateSimpleLoops(set<TransIdx> &acceleratedRules) {
    bool res = false;

    for (LocationIdx node : its.getLocations()) {
        res = Accelerator::accelerateSimpleLoops(its, node, acceleratedRules) || res;
        if (Timeout::soft()) return res;
    }

    return res;
}


bool Analysis::pruneRules() {
    // Always remove unreachable rules
    bool changed = Pruning::removeLeafsAndUnreachable(its);

    // Prune parallel transitions if enabled
    if (Config::Analysis::Pruning) {
        changed = Pruning::pruneParallelRules(its) || changed;
    }

    return changed;
}


// #############################
// ## Complexity Computation  ##
// #############################


option<RuntimeResult> Analysis::checkConstantComplexity() const {

    for (TransIdx idx : its.getTransitionsFrom(its.getInitialLocation())) {
        const Rule &rule = its.getRule(idx);
        GuardList guard = rule.getGuard();
        guard.push_back(rule.getCost() >= 1);

        if (Z3Toolbox::checkAll(guard) == z3::sat) {
            proofout.result("The following rule witnesses the lower bound Omega(1):");
            stringstream s;
            ITSExport::printLabeledRule(idx, its, s);
            proofout.append(s);

            RuntimeResult res;
            res.guard = rule.getGuard();
            res.cost = rule.getCost();
            res.solvedCost = rule.getCost();
            res.cpx = Complexity::Const;
            return {res};
        }
    }

    return {};
}


/**
 * Helper for getMaxRuntime that searches for the maximal cost.getComplexity().
 * Note that this does not involve the asymptotic bounds check and thus not give sound results!
 */
static RuntimeResult getMaxComplexityApproximation(const ITSProblem &its, set<TransIdx> rules) {
    RuntimeResult res;

    for (TransIdx rule : rules) {
        Complexity cpxRule = its.getRule(rule).getCost().getComplexity();
        if (cpxRule > res.cpx) {
            res.cpx = cpxRule;
            res.guard = its.getRule(rule).getGuard();
            res.cost = its.getRule(rule).getCost();
        }
    }

    return res;
}


RuntimeResult Analysis::getMaxRuntimeOf(const set<TransIdx> &rules, RuntimeResult currResult) {
    auto isTempVar = [&](const ExprSymbol &var){ return its.isTempVar(var); };

    // Only search for runtimes that improve upon the current runtime
    RuntimeResult res = currResult;
    vector<TransIdx> todo(rules.begin(), rules.end());

    // sort the rules before analyzing them
    // non-terminating rules first
    // non-polynomial (i.e., most likely exponential) rules second (preferring rules with temporary variables)
    // rules with temporary variables (sorted by their degree) third
    // rules without temporary variables (sorted by their degree) last
    // if rules are equal wrt. the criteria above, prefer those with less constraints in the guard
    auto comp = [this, isTempVar](const TransIdx &fst, const TransIdx &snd) {
        Rule fstRule = its.getRule(fst);
        Rule sndRule = its.getRule(snd);
        Expression fstCpxExp = fstRule.getCost().expand();
        Expression sndCpxExp = sndRule.getCost().expand();
        if (fstCpxExp != sndCpxExp) {
            if (fstCpxExp.isNontermSymbol()) return true;
            if (sndCpxExp.isNontermSymbol()) return false;
            bool fstIsNonPoly = !fstCpxExp.isPolynomial();
            bool sndIsNonPoly = !sndCpxExp.isPolynomial();
            if (fstIsNonPoly > sndIsNonPoly) return true;
            if (fstIsNonPoly < sndIsNonPoly) return false;
            bool fstHasTmpVar = fstCpxExp.hasVariableWith(isTempVar);
            bool sndHasTmpVar = sndCpxExp.hasVariableWith(isTempVar);
            if (fstHasTmpVar > sndHasTmpVar) return true;
            if (fstHasTmpVar < sndHasTmpVar) return false;
            Complexity fstCpx = fstCpxExp.getComplexity();
            Complexity sndCpx = sndCpxExp.getComplexity();
            if (fstCpx > sndCpx) return true;
            if (fstCpx < sndCpx) return false;
        }
        unsigned long fstGuardSize = fstRule.getGuard().size();
        unsigned long sndGuardSize = sndRule.getGuard().size();
        return fstGuardSize < sndGuardSize;
    };

    sort(todo.begin(), todo.end(), comp);

    for (TransIdx ruleIdx : todo) {
        Rule &rule = its.getRuleMut(ruleIdx);

        // getComplexity() is not sound, but gives an upperbound, so we can avoid useless asymptotic checks.
        // We have to be careful with temp variables, since they can lead to unbounded cost.
        const Expression &cost = rule.getCost();
        bool hasTempVar = !cost.isNontermSymbol() && cost.hasVariableWith(isTempVar);

        if (cost.getComplexity() <= max(res.cpx, Complexity::Const) && !hasTempVar) {
            continue;
        }

        proofout.newline();
        proofout.headline(stringstream() << "Computing asymptotic complexity for rule " << ruleIdx);

        // Simplify guard to speed up asymptotic check
        bool simplified = false;
        simplified |= Preprocess::simplifyGuard(rule.getGuardMut());
        simplified |= Preprocess::simplifyGuardBySmt(rule.getGuardMut());
        if (simplified) {
            proofout.append("Simplified the guard:");
            stringstream s;
            ITSExport::printLabeledRule(ruleIdx, its, s);
            proofout.append(s);
        }
        if (Timeout::hard()) break;

        // Perform the asymptotic check to verify that this rule's guard allows infinitely many models
        option<AsymptoticBound::Result> checkRes;
        bool isPolynomial = rule.getCost().isPolynomial() && !rule.getCost().isNontermSymbol();
        if (isPolynomial) {
            for (const Expression &e: rule.getGuard()) {
                if (!Expression(e.lhs()).isPolynomial() || !Expression(e.rhs()).isPolynomial()) {
                    isPolynomial = false;
                    break;
                }
            }
        }
        if (isPolynomial) {
            checkRes = AsymptoticBound::determineComplexityViaSMT(
                    its,
                    rule.getGuard(),
                    rule.getCost(),
                    true,
                    res.cpx);
        } else {
            checkRes = AsymptoticBound::determineComplexity(
                    its,
                    rule.getGuard(),
                    rule.getCost(),
                    true,
                    res.cpx);
        }

        proofout.append(stringstream() << "Resulting cost " << checkRes.get().solvedCost << " has complexity: " << checkRes.get().cpx);

        if (checkRes.get().cpx > res.cpx) {
            proofout.newline();
            proofout.result(stringstream() << "Found new complexity " << checkRes.get().cpx << ".");

            res.cpx = checkRes.get().cpx;
            res.solvedCost = checkRes.get().solvedCost;
            res.reducedCpx = checkRes.get().reducedCpx;
            res.guard = rule.getGuard();
            res.cost = rule.getCost();

            if (res.cpx >= Complexity::Unbounded) {
                break;
            }
        }

        if (Timeout::hard()) break;
    }

    return res;
}


RuntimeResult Analysis::getMaxRuntime() {
    auto rules = its.getTransitionsFrom(its.getInitialLocation());

    if (!Config::Analysis::AsymptoticCheck) {
        proofout.warning("WARNING: The asymptotic check is disabled, the result might be unsound!");
        return getMaxComplexityApproximation(its, rules);
    }

    RuntimeResult res = getMaxRuntimeOf(rules, RuntimeResult());

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


void Analysis::removeConstantPathsAfterTimeout() {
    set<LocationIdx> visited;
    removeConstantPathsImpl(its, its.getInitialLocation(), visited);
}


RuntimeResult Analysis::getMaxPartialResult() {
    RuntimeResult res;
    LocationIdx initial = its.getInitialLocation(); // just a shorthand

    // contract and always compute the maximum complexity to allow abortion at any time
    while (true) {
        if (Timeout::hard()) goto abort;

        // check runtime of all rules from the start state
        res = getMaxRuntimeOf(its.getTransitionsFrom(initial), res);

        // handle special cases to ensure termination in time
        if (res.cpx >= Complexity::Unbounded) goto done;
        if (Timeout::hard()) goto abort;

        // contract next level (if there is one), so we get new rules from the start state
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

                // We already computed the complexity and tried to chain, so we can drop this rule
                its.removeRule(first);
            }
        }
        proofout.headline("Performed chaining from the start location:");
    }

abort:
    proofout.append("Aborting due to timeout");
done:
    return res;
}
