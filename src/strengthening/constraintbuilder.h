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

        static const SmtConstraints build(
                const Templates &templates,
                const RuleContext &ruleCtx,
                const GuardContext &guardCtx,
                Z3Context &z3Ctx);

    private:

        const Templates &templates;
        const RuleContext &ruleCtx;
        const GuardContext &guardCtx;
        Z3Context &z3Ctx;

        ConstraintBuilder(
                const Templates &templates,
                const RuleContext &ruleCtx,
                const GuardContext &guardCtx,
                Z3Context &z3Ctx);

        const SmtConstraints build() const;

        const Implication buildTemplatesInvariantImplication() const;

        const Initiation constructInitiationConstraints(const GuardList &premise) const;

        const std::vector<z3::expr> constructImplicationConstraints(
                const GuardList &premise,
                const GuardList &conclusion) const;

        const z3::expr constructImplicationConstraints(
                const GuardList &premise,
                const Expression &conclusion) const;

        const GuardList findRelevantConstraints() const;

    };

}

#endif //LOAT_STRENGTHENING_CONSTRAINT_BUILDER_H
