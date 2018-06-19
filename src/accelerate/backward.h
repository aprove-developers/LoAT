#ifndef BACKWARDACCELERATION_H
#define BACKWARDACCELERATION_H

#include "its/itsproblem.h"
#include "its/rule.h"
#include "util/option.h"

class BackwardAcceleration {
public:
    static option<std::vector<LinearRule>> accelerate(VarMan &varMan, const LinearRule &rule);

private:
    BackwardAcceleration(VarMan &varMan, const LinearRule &rule);

    /**
     * Main function, just calls the methods below in the correct order
     */
    option<std::vector<LinearRule>> run();

    /**
     * Checks whether the backward acceleration technique might be applicable.
     */
    bool shouldAccelerate() const;

    /**
     * Given a dependency order for the rule's update, computes the inverse update (as substitution).
     * This may fail if the update contains nonlinear expressions.
     */
    option<GiNaC::exmap> computeInverseUpdate(const std::vector<VariableIdx> &order) const;

    /**
     * Checks (with a z3 query) if the guard is monotonic w.r.t. the given inverse update.
     */
    bool checkGuardImplication(const GiNaC::exmap &inverseUpdate) const;

    /**
     * Computes the accelerated rule from the given iterated update and cost, where N is the iteration counter.
     */
    LinearRule buildAcceleratedRule(const UpdateMap &iteratedUpdate, const Expression &iteratedCost,
                                    const ExprSymbol &N) const;

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
    static option<std::vector<Expression>> computeUpperbounds(const ExprSymbol &N, const GuardList &guard);

private:
    VariableManager &varMan;
    const LinearRule &rule;
};

#endif /* BACKWARDACCELERATION_H */
