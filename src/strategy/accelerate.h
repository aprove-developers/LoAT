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

#ifndef ACCELERATE_H
#define ACCELERATE_H

#include "global.h"

#include "graph/graph.h"
#include "its/itsproblem.h"
#include "expr/expression.h"

#include "accelerate/farkas.h"

#include <boost/optional.hpp>


class Accelerator {
public:
    /**
     * Replaces all simple loops of the given location with accelerated simple loops
     * by searching for metering functions and iterated costs/updates.
     *
     * Also handles nesting and chaining of parallel simple loops (where possible).
     *
     * @return true iff the ITS was modified
     *         (which is always the case if any simple loops were present)
     */
    static bool accelerateSimpleLoops(LinearITSProblem &its, LocationIdx loc);

private:
    // All data returned by FarkasMeterGenerator::generate
    struct MeteringResult {
        FarkasMeterGenerator::Result result;
        Expression meteringFunction;
        VariablePair conflictVar;
        LinearRule modifiedRule;
    };

    // Potential candidate for the inner loop when nesting two loops.
    // Inner loops are always accelerated loops, so this stores the original and the accelerated rule.
    struct InnerNestingCandidate {
        TransIdx oldRule;
        TransIdx newRule;
    };

    // Potential candidate for the outer loop when nesting two loops.
    // Outer loops are always original rules (before acceleration).
    struct OuterNestingCandidate {
        TransIdx oldRule;
    };

    // Potential candidate for the min-max heuristic for conflicting variables.
    // The heuristic should work around the issue that we do not support min/max(A,B) in metering functions.
    struct ConflictVarsCandidate {
        TransIdx oldRule;
        VariablePair conflictVars;
    };

private:
    Accelerator(LinearITSProblem &its, LocationIdx loc);


    /**
     * Helper that calls Preprocess::simplifyRule
     */
    static void simplifyRule(LinearRule &rule);

    /**
     * Helper that chains all pairs of loops at the given location.
     * Resulting rules are added to the ITS.
     */
    static void chainAllLoops(LinearITSProblem &its, LocationIdx loc);


    /**
     * Main function. Tries to accelerate and nest all loops.
     * This includes heuristics like min-max and guard strengthening.
     */
    void run();


    /**
     * Tries to find a metering function for the given rule
     */
    MeteringResult meter(const LinearRule &rule) const;

    /**
     * Processes the result of finding a metering function for the given rule.
     * If a metering function was found, the iterated cost and update are computed.
     * If successful, the rule is added to the ITS.
     *
     * Depending on the result, the rule is also added to some of the following members:
     * inner/outerNestingCandidates, rulesWithoutMetering, rulesWithConflictingVariables, keepRules.
     *
     * Depending on the result, the following flag is also set:
     * addEmptyRuleToSkipLoops.
     *
     * Note that most of the decisions are heuristics that try to find a suitable balance
     * between an exploding number of rules and discarding valuable rules.
     *
     * @returns true if acceleration was successful or we found an non-terminating loop
     */
    bool handleMeteringResult(TransIdx originalRule, MeteringResult meterResult);

    /**
     * Tries to accelerate the given loop by searching for a metering function
     * and then calling handleMeteringResult to decide what to do with the result.
     *
     * @param rule The rule to accelerate
     *
     * @param originalRule The index of the original rule in the ITS from which rule originates.
     * This index is passed to handleMeteringResults.
     *
     * @param storeOnlySuccessful If true, handleMeteringResult is only called
     * if a metering function was found or a non-terminating loop was recognized.
     *
     * @return the return value of handleMeteringResult (or false if not called)
     */
    bool accelerateAndStore(TransIdx originalRule, const LinearRule &rule, bool storeOnlySuccessful = false);

    /**
     * Tries to accelerate the given loop.
     * If acceleration is successful or we recognize nontermination, the resulting rule is returned.
     * Otherwise none is returned.
     *
     * No members are modified, so the rule is _not_ added to accelerateRules
     */
    boost::optional<LinearRule> accelerate(const LinearRule &rule) const;


    /**
     * Helper function that checks with a simple heuristic if the transitions might be nested loops
     * (this is done to avoid too many nesting attemts, as finding a metering function takes some time).
     */
    bool canNest(const LinearRule &inner, const LinearRule &outer) const;

    /**
     * Adds the given accelerated rule, which resulted from nesting the given inner and outer transitions.
     * Also tries to chain the given rule `chain` in front of `accelerated` and adds the result (if it works).
     * Also takes care of the proof output for nesting.
     */
    void addNestedRule(const LinearRule &accelerated, const LinearRule &chain, TransIdx inner, TransIdx outer, std::vector<InnerNestingCandidate> &nested);

    /**
     * Tries to nest the given nesting candidates (i.e., rules).
     * If successful, the rules are added to acceleratedRules and to the parameter nested.
     *
     * Note that several rules can be added in a single call, since different ways of nesting are tested.
     *
     * Returns true if nesting was successful.
     */
    bool nestRules(const InnerNestingCandidate &inner, const OuterNestingCandidate &outer,
                   std::vector<InnerNestingCandidate> &nested);

private:
    // All rules where acceleration failed, but where we want to keep the un-accelerated rule.
    std::set<TransIdx> keepRules;

    // Rules that are suitable candidates for Nesting
    std::vector<InnerNestingCandidate> innerNestingCandidates;
    std::vector<OuterNestingCandidate> outerNestingCandidates;

    // Rules where acceleration failed since no metering function was found (result was Unsat).
    // We might try to accelerate these rules again with some optimizations (e.g. guard strengthening).
    std::set<TransIdx> rulesWithUnsatMetering;

    // Rules where acceleration failed since no metering function was found due to conflicting variables.
    // We might try to accelerate these rules again after applying the min-max heuristic.
    // That is, if variables A and B are conflicting, we try adding "A < B" or "A > B" to the guard.
    std::vector<ConflictVarsCandidate> rulesWithConflictingVariables;

    // After acceleration, incoming loops are chained with all acclerated loops.
    // This assumes that a loop is always executed at least once.
    // In some cases (heuristic), it might be better to not execute any loop.
    // To support this, an empty loop is added in the end (parallel to the other accelerated loops),
    // so chaining with this loop has the same effect as executing none of the accelerated loops.
    bool addEmptyRuleToSkipLoops = false;

    // The ITS problem. Accelerated rules are added to the ITS immediately,
    // but no rules are removed until the very end (end of run()).
    LinearITSProblem &its;

    // The location for which simple loops shall be accelerated.
    LocationIdx targetLoc;
};

#endif // ACCELERATE_H
