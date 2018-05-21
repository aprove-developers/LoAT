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

#ifndef NL_PRUNE_H
#define NL_PRUNE_H

#include "graph/hypergraph.h"

class Rule;
class ITSProblem;


namespace PruningNL {
    /**
     * Tries to identify and remove duplicate transitions within the given list of transitions
     * @param trans list of transitions that are checked
     * @note does not catch all duplicates, as this is a purely syntactical check (no z3 calls)
     * @return true iff the ITS was modified (i.e. a duplicate got deleted)
     */
    bool removeDuplicateRules(ITSProblem &its, const std::vector<TransIdx> &trans, bool compareUpdate = true);

    /**
     * Checks initial rules (from the initial location) for satisfiability, removes unsat rules.
     * @return true iff the ITS was modified
     */
    bool removeUnsatInitialRules(ITSProblem &its);

    /**
     * Reduces the number of parallel rules by applying some greedy heuristic to find the "best" rules
     * @note also removes unreachable nodes and irrelevant const transitions using removeConstLeafsAndUnreachable()
     * @return true iff the ITS was modified (transitions were deleted)
     */
//    bool pruneParallelRules(LinearITSProblem &its); TODO: Port this

    /**
     * Removes all unreachable nodes and rules to leafs with constant cost, as they have no impact on the runtime
     * @return true iff the ITS was modified
     */
    bool removeLeafsAndUnreachable(ITSProblem &its);

    /**
     * A simple syntactic comparision. Returns true iff a and b are equal up to constants in the cost term.
     * @note as this is a syntactic check, false has no guaranteed meaning
     * @param compareUpdate if false, the update is not compared (i.e., rules with different update might be equal)
     * @return true if the rules are equal up to constants in the cost term, false if they differ or we are unsure.
     */
    bool compareRules(const Rule &a, const Rule &b, bool compareUpdate = true);
}

#endif // PRUNE_H
