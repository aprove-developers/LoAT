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
                GuardList simpleInvariants,
                GuardList decreasing,
                GuardList todo):
                guard(guard),
                invariants(std::move(invariants)),
                simpleInvariants(std::move(simpleInvariants)),
                decreasing(std::move(decreasing)),
                todo(std::move(todo)) { }

        const GuardList &guard;
        const GuardList invariants;
        const GuardList simpleInvariants;
        const GuardList decreasing;
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
        std::vector<z3::expr> hard;
        std::vector<z3::expr> soft;
    };

    struct Initiation {
        std::vector<z3::expr> valid;
        std::vector<z3::expr> satisfiable;
    };

    struct SmtConstraints {

        SmtConstraints(
                const Initiation &initiation,
                const std::vector<z3::expr> &templatesInvariant,
                const std::vector<z3::expr> &conclusionsInvariant,
                const std::vector<z3::expr> &conclusionsMonotonic) :
                initiation(std::move(initiation)),
                templatesInvariant(std::move(templatesInvariant)),
                conclusionsInvariant(std::move(conclusionsInvariant)),
                conclusionsMonotonic(std::move(conclusionsMonotonic)) {}

        const Initiation initiation;
        const std::vector<z3::expr> templatesInvariant;
        const std::vector<z3::expr> conclusionsInvariant;
        const std::vector<z3::expr> conclusionsMonotonic;

    };

    typedef std::function<const MaxSmtConstraints(const SmtConstraints &, bool, Z3Context &)> Mode;

}

#endif //LOAT_STRENGTHENING_TYPES_H
