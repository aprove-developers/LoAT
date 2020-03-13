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

#ifndef LOAT_STRENGTHENING_TYPES_H
#define LOAT_STRENGTHENING_TYPES_H


#include "../its/types.hpp"
#include "../its/rule.hpp"
#include "../expr/boolexpr.hpp"

namespace strengthening {

    struct RuleContext {

        RuleContext(
                const Rule &rule,
                const std::vector<Subs> &updates,
                VariableManager &varMan):
                rule(rule),
                updates(std::move(updates)),
                varMan(varMan) { }

        const Rule &rule;
        const std::vector<Subs> updates;
        VariableManager &varMan;
    };

    struct GuardContext {

        GuardContext(const GuardList &guard,
                GuardList invariants,
                GuardList todo):
                guard(guard),
                invariants(std::move(invariants)),
                todo(std::move(todo)) { }

        const GuardList &guard;
        const GuardList invariants;
        const GuardList todo;
    };

    struct Implication {
        GuardList premise;
        GuardList conclusion;
    };

    struct Result {
        GuardList solved;
        GuardList failed;
    };

    struct SmtConstraints {

        SmtConstraints(
                const BoolExpr &initiation,
                const BoolExpr &templatesInvariant,
                const BoolExpr &conclusionsInvariant) :
                initiation(std::move(initiation)),
                templatesInvariant(std::move(templatesInvariant)),
                conclusionsInvariant(std::move(conclusionsInvariant)) {}

        const BoolExpr initiation;
        const BoolExpr templatesInvariant;
        const BoolExpr conclusionsInvariant;

    };

}

#endif //LOAT_STRENGTHENING_TYPES_H
