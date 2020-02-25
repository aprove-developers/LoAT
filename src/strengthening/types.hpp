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
                const std::vector<GiNaC::exmap> &updates,
                const std::vector<GuardList> &preconditions,
                VariableManager &varMan):
                rule(rule),
                updates(std::move(updates)),
                preconditions(std::move(preconditions)),
                varMan(varMan) { }

        const Rule &rule;
        const std::vector<GiNaC::exmap> updates;
        const std::vector<GuardList> preconditions;
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

    struct Invariants {
        GuardList invariants;
        GuardList pseudoInvariants;
    };

    struct MaxSmtConstraints {
        BoolExpr hard = True;
        std::vector<BoolExpr> soft;
    };

    struct Initiation {
        BoolExpr valid;
        std::vector<BoolExpr> satisfiable;
    };

    struct SmtConstraints {

        SmtConstraints(
                const Initiation &initiation,
                const BoolExpr &templatesInvariant,
                const BoolExpr &conclusionsInvariant) :
                initiation(std::move(initiation)),
                templatesInvariant(std::move(templatesInvariant)),
                conclusionsInvariant(std::move(conclusionsInvariant)) {}

        const Initiation initiation;
        const BoolExpr templatesInvariant;
        const BoolExpr conclusionsInvariant;

    };

    typedef std::function<const MaxSmtConstraints(const SmtConstraints &)> Mode;

}

#endif //LOAT_STRENGTHENING_TYPES_H
