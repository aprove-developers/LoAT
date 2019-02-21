//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_CONSTRAINTSOLVER_H
#define LOAT_STRENGTHENING_CONSTRAINTSOLVER_H

#include "types.h"
#include "templates.h"

namespace strengthening {

    class ConstraintSolver {

    public:

        ConstraintSolver(
                const std::vector<GuardList> &preconditions,
                const MaxSmtConstraints &constraints,
                const Templates &templates,
                Z3Context &z3Context,
                VariableManager &varMan);

        const option<Invariants> solve() const;

    private:

        const std::vector<GuardList> &preconditions;
        const MaxSmtConstraints &constraints;
        const Templates &templates;
        Z3Context &z3Context;
        VariableManager &varMan;

        const GuardList instantiateTemplates(const z3::model &model) const;

        const Invariants splitInitiallyValid(const GuardList &invariants) const;

    };

}


#endif //LOAT_STRENGTHENING_CONSTRAINTSOLVER_H
