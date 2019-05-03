#ifndef BACKWARDACCELERATION_H
#define BACKWARDACCELERATION_H

#include "../its/itsproblem.hpp"
#include "../its/rule.hpp"
#include "../util/option.hpp"
#include "../accelerate/forward.hpp"

class BackwardAcceleration {
public:

    struct AcceleratedRules {
        const std::vector<Rule> rules;
        const unsigned int validityBound;
    };

    struct AccelerationResult {
        const std::vector<Rule> res;
        const ForwardAcceleration::ResultKind status;
    };

    static AccelerationResult accelerate(VarMan &varMan, const Rule &rule, const LocationIdx &sink);

private:
    BackwardAcceleration(VarMan &varMan, const Rule &rule, const LocationIdx &sink);

    /**
     * Main function, just calls the methods below in the correct order
     */
    AccelerationResult run();

    option<Rule> buildInit(unsigned int iterations) const;

    /**
     * Checks whether the backward acceleration technique might be applicable.
     */
    bool shouldAccelerate() const;

    /**
     * Checks (with a z3 query) if the guard is monotonic w.r.t. the given inverse update.
     */
    bool checkGuardImplication(const GuardList &reducedGuard, const GuardList &irrelevantGuard) const;

    /**
     * Computes the accelerated rule from the given iterated update and cost, where N is the iteration counter.
     */
    Rule buildAcceleratedLoop(const UpdateMap &iteratedUpdate, const Expression &iteratedCost,
                              const GuardList &guard, const ExprSymbol &N) const;

    Rule buildNontermRule() const;

    Rule buildAcceleratedRecursion(const std::vector<UpdateMap> &iteratedUpdates, const Expression &iteratedCost,
                                   const GuardList &guard, const ExprSymbol &N, unsigned int validityBound) const;

    bool checkCommutation(const std::vector<UpdateMap> &updates);

    GuardList selfContainedGuard(const GuardList &irrelevantGuard) const;

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
    static std::vector<Rule> replaceByUpperbounds(const ExprSymbol &N, const Rule &rule);

    /**
     * Helper for replaceByUpperbounds, returns all upperbounds of N in rule's guard,
     * or fails if not all of them can be computed.
     */
    static std::vector<Expression> computeUpperbounds(const ExprSymbol &N, const GuardList &guard);

private:
    VariableManager &varMan;
    const Rule &rule;
    const LocationIdx &sink;
};

#endif /* BACKWARDACCELERATION_H */
