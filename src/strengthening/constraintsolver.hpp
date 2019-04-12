//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_CONSTRAINT_SOLVER_H
#define LOAT_STRENGTHENING_CONSTRAINT_SOLVER_H

#include "types.hpp"
#include "templates.hpp"

namespace strengthening {

    class ConstraintSolver {

    public:

        static const option<Invariants> solve(
                const RuleContext &ruleCtx,
                const MaxSmtConstraints &constraints,
                const Templates &templates,
                Z3Context &z3Ctx);

    private:

        const RuleContext &ruleCtx;
        const MaxSmtConstraints &constraints;
        const Templates &templates;
        Z3Context &z3Ctx;

        ConstraintSolver(
                const RuleContext &ruleCtx,
                const MaxSmtConstraints &constraints,
                const Templates &templates,
                Z3Context &z3Ctx);

        const option<Invariants> solve() const;

        const GuardList instantiateTemplates(const z3::model &model) const;

        const option<Invariants> splitInitiallyValid(const GuardList &invariants) const;

    };

}


#endif //LOAT_STRENGTHENING_CONSTRAINT_SOLVER_H
