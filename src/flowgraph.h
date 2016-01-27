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

#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

#include "global.h"

#include "graph.h"
#include "itrs.h"
#include "expression.h"


/**
 * Represents one transition in the graph with the given target, guards and updates
 */
struct Transition {
    Transition() : cost(1) {}
    GuardList guard;
    UpdateMap update;
    Expression cost;
};

std::ostream& operator<<(std::ostream &, const Transition &);



struct RuntimeResult {
    Expression bound;
    GuardList guard;
    Complexity cpx;
    bool reducedCpx;
    RuntimeResult() : bound(0), cpx(Expression::ComplexNone), reducedCpx(false) {}
};

/**
 * Flow graph for an ITRS
 */
class FlowGraph : private Graph<Transition> {
public:
    /**
     * Creates the flow graph for the given itrs
     */
    FlowGraph(ITRSProblem &itrs);

    /**
     * Prints the graph in a readable but ugly format for debugging
     * @param s the output stream to print to (e.g. cout)
     */
    void print(std::ostream &s) const;

    void printForProof() const;

    void printDot(std::ostream &s, int step, const std::string &desc) const;
    void printDotText(std::ostream &s, int step, const std::string &desc) const;

    void printT2(std::ostream &s) const;

    /**
     * Returns true if there are no (reachable) transitions
     */
    bool isEmpty() const;

    //calls Preprocess::...)
    bool simplifyTransitions();

    /**
     * Checks initial transitions for satisfiability, removes unsat transitions.
     * @return true iff the graph was modified
     */
    bool reduceInitialTransitions();

    /**
     * Apply simple chaining (i.e. only linear paths)
     * @return true iff the graph was modified
     */
    bool contract();

    /**
     * Apply branched chaining (i.e. the eliminated node can have multiple outgoing edges)
     * @note this is quite powerful, but often creates many branches. Consider pruning afterwards
     * @return true iff the graph was modified
     */
    bool contractBranches();

    /**
     * Replaces all selfloops with linear transitions by searching for metering functions and iterated costs/updates.
     * Also handles nesting and chaining of parallel selfoops (where possible).
     * @return true iff the graph was modified (which is always the case if any selfloops were present)
     */
    bool removeSelfloops();

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

    //call these in the following order
    bool isFullyContracted() const;
    RuntimeResult getMaxRuntime();

    //to recover in case of a timeout
    bool removeIrrelevantTransitions(NodeIndex curr, std::set<NodeIndex> &visited);
    RuntimeResult getMaxPartialResult();

private:
    /**
     * Adds the given rule to this graph, calculating the required update
     */
    void addRule(const Rule &rule);

    NodeIndex addNode();

    /**
     * Contracts transition followTrans into trans
     * @param trans the first transition, will be modified
     * @param followTrans the second transition. Must follow trans!
     * @note it is valid for trans and followTrans to point to the same data
     * @note does only affect trans, the internal transitions are *not* modified
     * @return true iff contraction was performed, false if aborted as result was not SATable
     */
    bool contractTransitionData(Transition &trans, const Transition &followTrans) const;

    bool contractLinearPaths(NodeIndex node, std::set<NodeIndex> &visited);

    bool contractBranchedPaths(NodeIndex node, std::set<NodeIndex> &visited);

    bool canNest(const Transition &inner, const Transition &outer) const;

    bool removeSelfloopsFrom(NodeIndex node);

    bool compareTransitions(TransIndex a, TransIndex b) const;

    bool removeConstLeafsAndUnreachable();

private:
    NodeIndex initial;
    std::set<NodeIndex> nodes;
    NodeIndex nextNode;

    ITRSProblem &itrs;
};

#endif // FLOWGRAPH_H
