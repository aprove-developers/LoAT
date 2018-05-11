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

#ifndef NL_CHAINING_H
#define NL_CHAINING_H

#include "global.h"

#include <boost/optional.hpp>

#include "graph/graph.h"
#include "its/itsproblem.h"
#include "expr/expression.h"


namespace ChainingNL {

    /**
     * Performs one chaining step by chaining the two given rules
     * Returns the resulting rule, unless it can be shown to be unsatisfiable.
     */
    // chain the specified rhs of first with second.
    boost::optional<NonlinearRule> chainRulesOnRhs(const VarMan &varMan, const NonlinearRule &first, int firstRhsIdx, const NonlinearRule &second);

    // chain all rhss of first leading to second's lhs location with second.
    boost::optional<NonlinearRule> chainRules(const VarMan &varMan, const NonlinearRule &first, const NonlinearRule &second);

    /**
     * Applies a simple chaining strategy to the entire ITS problem.
     *
     * Starting from the initial node, all "linear paths" are chained (in a DFS traversal).
     * Here, "linear path" is a path where each node has at most one incoming and outgoing edge.
     * Such pathes have no interesting control flow and can safely be chained into a single edge.
     *
     * After calling this, all nodes with exactly one in- and one outgoing edge have either been
     * (1) eliminated, if chaining the in- and outgoing edge was possible, or
     * (2) their outgoing edge was removed, if chaining was not possible (so the edge can never be taken).
     *
     * @return true iff the ITS was modified
     */
    bool chainLinearPaths(ITSProblem &its);

    /**
     * Applies a more involved chaining strategy to the entire ITS problem.
     *
     * In contrast to chainLinearPaths, this also eliminates nodes with multiple outgoing edges
     * However, nodes with several distinct predecessors are *not* eliminated. The idea is that such
     * nodes are often the head of a loop, so eliminating them would destroy the loop structure
     * (this is, of course, only a heuristic argument).
     *
     * As for chainLinearPaths, edges which cannot be chained are deleted, since they can never be taken.
     *
     * @note This is quite powerful, but often creates many branches. Consider pruning afterwards.
     *
     * @return true iff the ITS was modified
     */
    bool chainTreePaths(ITSProblem &its);

    /**
     * Starting from the initial location and performing a DFS traversal,
     * eliminates the first applicable node by chaining and stops.
     * Returns true iff an applicable node was found (and thus eliminated).
     *
     * A node is applicable for elimination if it has no simple loops,
     * has both in- and outgoing transitions and is not the initial location.
     *
     * @return true iff the ITS was modified
     */
    bool eliminateALocation(ITSProblem &its);

    /**
     * Chains all rules of the given vector (the list of successfully accelerated rules)
     * with their predecessors (if possible), unless the predecessor is itself an accelerated rule.
     * All rules of the given vector are removed afterwards.
     * Should be called directly after acceleration.
     *
     * Note: Accelerated rules are never chained with other accelerated rules.
     * Note: The predecessor rules (that are chained) are _not_ removed.
     *
     * @return true iff acceleratedLoops was non-empty (the return value is just for convenience)
     */
    bool chainAcceleratedLoops(ITSProblem &its, const std::set<TransIdx> &acceleratedLoops);

};

#endif // CHAINING_H
