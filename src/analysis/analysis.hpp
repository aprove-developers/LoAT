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

#ifndef LINEAR_H
#define LINEAR_H

#include "../global.hpp"

#include "../its/itsproblem.hpp"
#include "../expr/expression.hpp"

#include <fstream>


/**
 * Represents the final runtime complexity result,
 * including the final cost and guard
 */
struct RuntimeResult {
    // The final complexity (computed from bound and guard)
    Complexity cpx;

    // The final cost expression, after solving by asymptotic check
    Expression solvedCost;

    // The final cost, before solving
    Expression cost;

    // The final guard
    GuardList guard;

    // If false, cpx is the complexity of bound.
    // If true, the complexity had to be reduced to satisfy the guard (e.g. cost x and guard x = y^2).
    bool reducedCpx;

    // Default constructor yields unknown complexity
    RuntimeResult() : cpx(Complexity::Unknown), solvedCost(0), cost(0), reducedCpx(false) {}
};


/**
 * Analysis of ITSProblems where all rules are linear.
 * This class controls how chaining, acceleration and pruning are applied.
 */
class Analysis {
public:
    static RuntimeResult analyze(ITSProblem &its);

private:
    explicit Analysis(ITSProblem &its);

    /**
     * Main analysis algorithm.
     * Combines chaining, acceleration, pruning in some sensible order.
     */
    RuntimeResult run();

    /**
     * Makes sure that the cost of a rule is always nonnegative when the rule is applicable
     * by adding "cost >= 0" to each rule's guard (unless this is trivially true).
     * @note Does not check if "cost >= 0" is implied by the guard (should be covered by preprocessing)
     * @return true iff any rule was modified.
     */
    bool ensureNonnegativeCosts();

    /**
     * Makes sure the initial location has no incoming rules (by adding a new one, if required).
     * Returns true iff the ITS was modified (a new initial location was added).
     */
    bool ensureProperInitialLocation();

    /**
     * Removes all rules whose guard can be proven unsatisfiable by z3.
     * Note that this involves many z3 queries!
     * @return true iff the ITS was modified (a rule was removed)
     */
    bool removeUnsatRules();

    /**
     * Performs extensive preprocessing to simplify the ITS (i.e. remove unreachable nodes, simplify guards)
     * @note this is a slow operation and should be used rarely (e.g. only once before the processing begins)
     * @param eliminateCostConstraints if true, "cost >= 0" is removed from the guard if it is implied by the guard
     * @return true iff the ITS was modified
     */
    bool preprocessRules();

    /**
     * Returns true iff all all rules start from the initial state.
     */
    bool isFullySimplified() const;

    // Wrapper methods for Chaining/Accelerator/Pruning methods (adding statistics, debug output)
    bool chainLinearPaths();
    bool chainTreePaths();
    bool eliminateALocation(std::string &eliminatedLocation);
    bool chainAcceleratedLoops(const std::set<TransIdx> &acceleratedRules);
    bool accelerateSimpleLoops(std::set<TransIdx> &acceleratedRules);
    bool pruneRules();

    /**
     * Checks if there is a satisfiable initial rule with cost >= 1.
     * This ensures Omega(1), but is not a complete check (one could have an initial rule with cost 0).
     *
     * @return If a satisfiable rule is found, returns the corresponding runtime result.
     */
    option<RuntimeResult> checkConstantComplexity() const;

    /**
     * For a fully chained ITS problem, this calculates the maximum runtime complexity (using asymptotic bounds)
     */
    RuntimeResult getMaxRuntime();

    /**
     * In case of a timeout (when the ITS is not fully chained), this tries to find a good partial result.
     *
     * To this end, the complexity of all rules from the initial location is computed (using asymptotic bounds),
     * Then, these rules are chained with their successors and the process is repeated.
     * This way, complexity results are quickly obtained and deeper rules are considered if enough time is left.
     */
    RuntimeResult getMaxPartialResult();

    /**
     * Used by getMaxRuntime and getMaxPartialResult.
     * Computes the runtime of the given rules (using asymptotic bounds) and returns the maximum
     * computed runtime or currResult, whichever is larger. If currResult is given, rules whose
     * complexity cannot be larger than currResult are skipped to make the computation faster.
     */
    RuntimeResult getMaxRuntimeOf(const std::set<TransIdx> &rules, RuntimeResult currResult);

    /**
     * This removes all subgraphs where all rules only have constant/unknown cost (this includes simple loops!).
     * This is meant to be called if a (soft) timeout occurrs, to focus on rules with higher complexity.
     */
    void removeConstantPathsAfterTimeout();

    /**
     * Prints the ITS problem to the proof output and, if dot output is enabled,
     * to the dot output stream using the given descriptive text.
     */
    void printForProof(const std::string &dotDescription);

    /**
     * Prints the final complexity result with all relevant information to the proof output
     */
    void printResult(const RuntimeResult &runtime);

    // Handling of dot export
    void setupDotOutput();
    void finalizeDotOutput(const RuntimeResult &runtime);

private:
    ITSProblem &its;

    // State for dot export (file stream and number of written subgraphs, since they have to be numbered)
    uint dotCounter = 0;
    std::ofstream dotStream;
};

#endif // LINEAR_H
