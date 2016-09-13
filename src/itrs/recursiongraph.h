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

#ifndef RECURSIONGRAPH_H
#define RECURSIONGRAPH_H

#include "global.h"

#include "graph.h"
#include "itrsproblem.h"
#include "expression.h"

typedef int RightHandSideIndex;

struct RightHandSide;

/**
 * Represents one transition in the graph with the given target, guards and updates
 */
struct Transition {
    Transition() : cost(1) {}
    GuardList guard;
    UpdateMap update;
    Expression cost;

    Expression outerUpdate;
    int numberOfCalls;
    ExprSymbol outerUpdateVar;

    RightHandSide toRightHandSide(const ITRSProblem &itrs, FunctionSymbolIndex funSym) const;
};

std::ostream& operator<<(std::ostream &, const Transition &);

struct RightHandSide {
    TT::ExpressionVector guard;
    TT::Expression term;
    Expression cost;

    bool isLegacyTransition(const ITRSProblem &itrs) const;

    Transition toLegacyTransition(const ITRSProblem &itrs,
                                  const ExprSymbol &outerUpVar,
                                  FunctionSymbolIndex funSym) const;
    void substitute(const GiNaC::exmap &sub);
};

std::ostream& operator<<(std::ostream &os, const RightHandSide &rhs);

/**
 * Represents the final runtime complexity result,
 * including the final cost and guard
 */
struct RuntimeResult {
    Expression bound;
    GuardList guard;
    Complexity cpx;
    bool reducedCpx;
    RuntimeResult() : bound(0), cpx(Expression::ComplexNone), reducedCpx(false) {}
};

/**
 * Flow graph for an ITS.
 * This class implements the main logic of chaining and metering.
 */
class RecursionGraph : private Graph<RightHandSideIndex> {
public:
    /**
     * Creates the flow graph for the given its
     */
    RecursionGraph(ITRSProblem &its);

    bool solveRecursion(NodeIndex node);

    /**
     * Prints the graph in a readable but ugly format for debugging
     * @param s the output stream to print to (e.g. cout)
     */
    void print(std::ostream &s) const;

    /**
     * Print the graph in a more readable format suitable for the proof output
     */
    void printForProof() const;

    /**
     * Print the graph as dot outpus
     * @param s the output stream to print to (the dotfile)
     * @param step the number of the subgraph (should increase for every call)
     * @param desc a description of the current subgraph
     */
    void printDot(std::ostream &s, int step, const std::string &desc) const;
    void printDotText(std::ostream &s, int step, const std::string &desc) const;

    /**
     * Returns true if there are no (reachable) transitions from the initial location
     */
    bool isEmpty() const;

    //calls Preprocess::...)
    /**
     * Performs extensive preprocessing to simplify the graph (i.e. remove unreachable nodes, simplify guards)
     * @note this is a slow operation and should be used rarely (e.g. only once before the processing begins)
     * @return true iff the graph was modified
     */
    bool simplifyTransitions();

    /**
     * Tries to identify and remove duplicate transitions
     * @param trans list of transitions that are checked
     * @note does not catch all duplicates, as this is a purely syntactical check (no z3 calls)
     * @return true iff the graph was modified (i.e. a duplicate got deleted)
     */
    bool removeDuplicateTransitions(const std::vector<TransIndex> &trans);

    /**
     * Reduces the number of parallel transitions by applying some greedy heuristic to find the "best" transitions
     * @note also removes unreachable nodes and irrelevant const transitions using removeConstLeafsAndUnreachable()
     * @return true iff the graph was modified (transitions were deleted)
     */
    bool pruneTransitions();

    /**
     * Returns true iff all paths have a length of at most 1
     */
    bool isFullyChained() const;

    /**
     * For a fully chained graph problem, this calculates the maximum runtime complexity (using the infinity check)
     */
    RuntimeResult getMaxRuntime();

    /**
     * In case of a timeout (when the graph is not fully chained), this tries to find a good partial result at least
     */
    RuntimeResult getMaxPartialResult();

    /**
     * Checks initial transitions for satisfiability, removes unsat transitions.
     * @return true iff the graph was modified
     */
    bool reduceInitialTransitions();

    /**
     * Apply simple chaining (i.e. only linear paths)
     * @return true iff the graph was modified
     */
    bool chainLinear();

    /**
     * Eliminates a single location without simple loops by chaining incoming and outgoing
     * transitions.
     * @return true iff the graph was modified
     */
    bool eliminateALocation();

    /**
     * Apply branched chaining (i.e. the eliminated node can have multiple outgoing edges)
     * @note this is quite powerful, but often creates many branches. Consider pruning afterwards
     * @return true iff the graph was modified
     */
    bool chainBranches();

    /**
     * Apply chaining to simple loops
     * @return true iff the graph was modified
     */
    bool chainSimpleLoops();

    /**
     * Replaces all simple loops with accelerated simple loops
     * by searching for metering functions and iterated costs/updates.
     * Also handles nesting and chaining of parallel simple loops (where possible).
     * @return true iff the graph was modified
     *         (which is always the case if any simple loops were present)
     */
    bool accelerateSimpleLoops();

    bool solveRecursions();

private:

    /**
     * Adds the given rule to this graph, calculating the required update
     */
    void addRule(const Rule &rule);

    TransIndex addLegacyTransition(NodeIndex from,
                                   NodeIndex to,
                                   const Transition &trans);

    TransIndex addLegacyRightHandSide(NodeIndex from,
                                      NodeIndex to,
                                      const RightHandSide &trans);

    RightHandSideIndex addRightHandSide(NodeIndex node, const RightHandSide &rhs);
    RightHandSideIndex addRightHandSide(NodeIndex node, const RightHandSide &&rhs);

    bool hasExactlyOneRightHandSide(NodeIndex node);

    void removeRightHandSide(NodeIndex node, RightHandSideIndex rhs);

    std::set<NodeIndex> getSuccessorsOfRhs(const RightHandSide &rhs);

    /**
     * Chains transition followTrans into trans
     * @param trans the first transition, will be modified
     * @param followTrans the second transition. Must follow trans!
     * @note it is valid for trans and followTrans to point to the same data
     * @note does only affect trans, the internal transitions are *not* modified
     * @return true iff contraction was performed, false if aborted as result was not SATable
     */
    bool chainRightHandSides(RightHandSide &rhs,
                             const FunctionSymbolIndex funSymbolIndex,
                             const RightHandSide &followRhs) const;

    /**
     * Internal function for chainLinear
     * @return true iff the graph was modified
     */
    bool chainLinearPaths(NodeIndex node, std::set<NodeIndex> &visited);

    /**
     * Internal function for eliminateALocation
     * @return true iff the graph was modified
     */
    bool eliminateALocation(NodeIndex node, std::set<NodeIndex> &visited);

    /**
     * Internal function for chainBranches
     * @return true iff the graph was modified
     */
    bool chainBranchedPaths(NodeIndex node, std::set<NodeIndex> &visited);

    /**
     * Helper function that checks with a very simple heuristic if the transitions might be nested loops
     * (this is done to avoid too many nesting attemts, as finding a metering function takes some time).
     */
    bool canNest(const RightHandSide &inner, FunctionSymbolIndex index, const RightHandSide &outer) const;

    /**
     * Apply chaining to the simple loops of node
     * @return true iff the graph was modified
     */
    bool chainSimpleLoops(NodeIndex node);

    /**
     * Replaces all simple loops of the given location with accelerated simple loops
     * by searching for metering functions and iterated costs/updates.
     * Also handles nesting and chaining of parallel simple loops (where possible).
     * @return true iff the graph was modified
     *         (which is always the case if any selfloops were present)
     */
    bool accelerateSimpleLoops(NodeIndex node);


    void removeIncorrectTransitionsToNullNode();

    /**
     * A simple syntactic comparision. If it returns true, a and b are guaranteed to be equal.
     * @note as this is a syntactic check, false has no guaranteed meaning
     */
    bool compareRightHandSides(RightHandSideIndex a, RightHandSideIndex b) const;

    /**
     * Removes all unreachable nodes and transitions to leaves with constant cost, as they have no impact on the runtime
     * @return true iff the graph was modified
     */
    bool removeConstLeavesAndUnreachable();

    /**
     * This removes all subgraphs that have only constant costs.
     * @note this will also remove selfloops and is only meant to be used after a soft timeout
     * @return true iff the graph was modified
     */
    bool removeIrrelevantTransitions(NodeIndex curr, std::set<NodeIndex> &visited);

    bool rightHandSideIsEmpty(NodeIndex node, const RightHandSide &rhs) const;

private:
    ITRSProblem &itrs;

    NodeIndex initial;
    static const NodeIndex NULLNODE;
    std::set<NodeIndex> nodes;
    std::map<RightHandSideIndex,RightHandSide> rightHandSides;
    int nextRightHandSide;

    // accelerateSimpleLoops() uses the following set to communicate
    // with chainSimpleLoops().
    std::set<NodeIndex> addTransitionToSkipLoops;

    // special variable used when handling the outer update
    ExprSymbol outerUpdateVar;
};

#endif // RECURSIONGRAPH_H
