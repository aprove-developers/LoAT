#ifndef BACKWARDACCELERATION_H
#define BACKWARDACCELERATION_H

#include "its/itsproblem.h"
#include "its/rule.h"
#include <boost/optional.hpp>

class BackwardAcceleration {
public:
    static boost::optional<LinearRule> accelerate(VarMan &varMan, const LinearRule &rule);

private:
    BackwardAcceleration(VarMan &varMan, const LinearRule &rule);

    /**
     * Main function, just calls the methods below in the correct order
     */
    boost::optional<LinearRule> run();

    /**
     * Checks whether the backward acceleration technique might be applicable.
     */
    bool shouldAccelerate() const;

    /**
     * Given a dependency order for the rule's update, computes the inverse update (as substitution).
     * This may fail if the update contains nonlinear expressions.
     */
    boost::optional<GiNaC::exmap> computeInverseUpdate(const std::vector<VariableIdx> &order) const;

    /**
     * Checks (with a z3 query) if the guard is monotonic w.r.t. the given inverse update.
     */
    bool checkGuardImplication(const GiNaC::exmap &inverseUpdate) const;

    /**
     * Computes the accelerated rule from the given iterated update and cost, where N is the iteration counter.
     */
    LinearRule buildAcceleratedRule(const UpdateMap &iteratedUpdate, const Expression &iteratedCost,
                                    const ExprSymbol &N) const;

private:
    VariableManager &varMan;
    const LinearRule &rule;
};

#endif /* BACKWARDACCELERATION_H */
