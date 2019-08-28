/*  This file is part of LoAT.
 *  Copyright (c) 2018-2019 Florian Frohn
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

#ifndef BACKWARDACCELERATION_H
#define BACKWARDACCELERATION_H

#include "../its/itsproblem.hpp"
#include "../its/rule.hpp"
#include "../util/option.hpp"
#include "../accelerate/forward.hpp"

class BackwardAcceleration {
public:

    struct AcceleratedRules {
        const std::vector<LinearRule> rules;
        const unsigned int validityBound;
    };

    struct AccelerationResult {
        const std::vector<LinearRule> res;
        const ForwardAcceleration::ResultKind status;
    };

    static AccelerationResult accelerate(VarMan &varMan, const LinearRule &rule, const LocationIdx &sink);

private:
    BackwardAcceleration(VarMan &varMan, const LinearRule &rule, const LocationIdx &sink);

    void computeInvarianceSplit();

    /**
     * Main function, just calls the methods below in the correct order
     */
    AccelerationResult run();

    /**
     * Checks whether the backward acceleration technique might be applicable.
     */
    bool shouldAccelerate() const;

    /**
     * Checks (with a z3 query) if the guard is monotonic w.r.t. the given inverse update.
     */
    bool checkMonotonicity() const;

    bool checkEventualMonotonicity() const;

    bool checkMonotonicDecreasingness() const;

    /**
     * Computes the accelerated rule from the given iterated update and cost, where N is the iteration counter.
     */
    LinearRule buildAcceleratedLoop(const UpdateMap &iteratedUpdate, const Expression &iteratedCost,
                              const GuardList &strengthenedGuard, const ExprSymbol &N,
                              const unsigned int validityBound) const;

    LinearRule buildNontermRule() const;

    /**
     * If possible, replaces N by all its upper bounds from the guard of the given rule.
     * For every upper bound, a separate rule is created.
     *
     * If this is not possible (i.e., there is at least one upper bound that is too difficult
     * to compute like N^2 <= X or there are too many upper bounds), then N is not replaced
     * and a vector consisting only of the given rule is returned.
     *
     * @return A list of rules, either with N eliminated or only containing the given rule
     */
    static std::vector<LinearRule> replaceByUpperbounds(const ExprSymbol &N, const LinearRule &rule);

    /**
     * Helper for replaceByUpperbounds, returns all upperbounds of N in rule's guard,
     * or fails if not all of them can be computed.
     */
    static std::vector<Expression> computeUpperbounds(const ExprSymbol &N, const GuardList &guard);

private:
    VariableManager &varMan;
    const LinearRule &rule;
    const LocationIdx &sink;
    GuardList simpleInvariants;
    GuardList conditionalInvariants;
    GuardList nonInvariants;
    GuardList eventualInvariants;
    UpdateMap update;
    GiNaC::exmap updateSubs;
    GuardList guard;
};

#endif /* BACKWARDACCELERATION_H */
