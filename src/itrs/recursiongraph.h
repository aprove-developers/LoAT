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
#include "itrs.h"
#include "expression.h"

typedef int RightHandSideIndex;

struct RightHandSide {
    TT::ExpressionVector guard;
    TT::Expression term;
    TT::Expression cost;
};

std::ostream& operator<<(std::ostream &os, const RightHandSide &rhs);

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

private:

    /**
     * Adds the given rule to this graph, calculating the required update
     */
    void addRule(const ITRSRule &rule);

    RightHandSideIndex addRightHandSide(NodeIndex node, const RightHandSide &rhs);
    RightHandSideIndex addRightHandSide(NodeIndex node, const RightHandSide &&rhs);

    bool hasExactlyOneRightHandSide(NodeIndex node);

    void removeRightHandSide(NodeIndex node, RightHandSideIndex rhs);

    std::set<NodeIndex> getSuccessorsOfExpression(const TT::Expression &ex);

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
     * Apply chaining to the simple loops of node
     * @return true iff the graph was modified
     */
    bool chainSimpleLoops(NodeIndex node);

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
};

#endif // RECURSIONGRAPH_H
