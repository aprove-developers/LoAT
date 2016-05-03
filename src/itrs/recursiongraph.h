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

private:

    /**
     * Adds the given rule to this graph, calculating the required update
     */
    void addRule(const ITRSRule &rule);

private:
    ITRSProblem &itrs;

    NodeIndex initial;
    static const NodeIndex NULLNODE;
    std::set<NodeIndex> nodes;
    std::map<RightHandSideIndex,RightHandSide> rightHandSides;
    int nextRightHandSide;
};

#endif // RECURSIONGRAPH_H
