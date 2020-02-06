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

#include "../its/rule.hpp"
#include "../its/variablemanager.hpp"
#include "types.hpp"
#include "modes.hpp"
#include "templates.hpp"

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

        const Initiation constructInitiationConstraints(const GuardList &relevantConstraints) const;

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
