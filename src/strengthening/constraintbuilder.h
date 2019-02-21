//
// Created by ffrohn on 2/8/19.
//

#ifndef LOAT_STRENGTHENING_CONSTRAINT_BUILDER_H
#define LOAT_STRENGTHENING_CONSTRAINT_BUILDER_H

#include <its/rule.h>
#include <its/variablemanager.h>
#include "types.h"
#include "modes.h"
#include "templates.h"

namespace strengthening {

    class ConstraintBuilder {

    public:

        ConstraintBuilder(
                const Templates &templates,
                const GuardList &relevantConstraints,
                const Context &context,
                Z3Context &z3Context
        );

        const SmtConstraints buildSMTConstraints() const;

    private:

        const Templates &templates;
        const GuardList &relevantConstraints;
        const Context &context;
        Z3Context &z3Context;

        const Implication buildTemplatesInvariantImplication() const;

        const Initiation constructSmtInitiationConstraints(const GuardList &premise) const;

        const std::vector<z3::expr> constructImplicationConstraints(
                const GuardList &premise,
                const GuardList &conclusion) const;

        const z3::expr constructImplicationConstraints(
                const GuardList &premise,
                const Expression &conclusion) const;

    };

}

#endif //LOAT_STRENGTHENING_CONSTRAINT_BUILDER_H
