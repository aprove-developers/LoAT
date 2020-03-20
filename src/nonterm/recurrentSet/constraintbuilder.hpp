/*  This file is part of LoAT.
 *  Copyright (c) 2019 Florian Frohn
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

#ifndef LOAT_STRENGTHENING_CONSTRAINT_BUILDER_H
#define LOAT_STRENGTHENING_CONSTRAINT_BUILDER_H

#include "../../its/rule.hpp"
#include "../../its/variablemanager.hpp"
#include "types.hpp"
#include "templates.hpp"
#include "../../expr/boolexpr.hpp"

namespace strengthening {

    class ConstraintBuilder {

    public:

        static const BoolExpr buildSmtConstraints(const Templates &templates,
                                                  const RuleContext &ruleCtx,
                                                  const GuardContext &guardCtx);

    private:

        const Templates &templates;
        const RuleContext &ruleCtx;
        const GuardContext &guardCtx;

        ConstraintBuilder(
                const Templates &templates,
                const RuleContext &ruleCtx,
                const GuardContext &guardCtx);

        const Implication buildTemplatesInvariantImplication() const;

        const BoolExpr constructInitiationConstraints(const Guard &relevantConstraints) const;

        const BoolExpr constructImplicationConstraints(
                const Guard &premise,
                const Guard &conclusion) const;

        const BoolExpr constructImplicationConstraints(
                const Guard &premise,
                const Rel &conclusion) const;

        const Guard findRelevantConstraints() const;

        const SmtConstraints buildSmtConstraints() const;

    };

}

#endif //LOAT_STRENGTHENING_CONSTRAINT_BUILDER_H
