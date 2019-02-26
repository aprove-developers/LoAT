//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_TEMPLATE_BUILDER_H
#define LOAT_STRENGTHENING_TEMPLATE_BUILDER_H

#include <its/variablemanager.h>
#include <its/rule.h>
#include "types.h"
#include "templates.h"

namespace strengthening {

    class TemplateBuilder {

    public:

        static const Templates build(const GuardContext &guardCtx, const RuleContext &ruleCtx);

    private:

        const GuardContext &guardCtx;
        const RuleContext &ruleCtx;

        TemplateBuilder(const GuardContext &guardCtx, const RuleContext &ruleCtx);

        const Templates build() const;

        const ExprSymbolSet findRelevantVariables(const Expression &c) const;

        const Templates::Template buildTemplate(const ExprSymbolSet &vars) const;

    };

}

#endif //LOAT_STRENGTHENING_TEMPLATE_BUILDER_H
