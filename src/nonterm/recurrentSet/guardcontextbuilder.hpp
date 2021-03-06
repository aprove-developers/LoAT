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

#ifndef LOAT_STRENGTHENING_GUARD_CONTEXT_BUILDER_H
#define LOAT_STRENGTHENING_GUARD_CONTEXT_BUILDER_H


#include "../../its/types.hpp"
#include "types.hpp"

namespace strengthening {

    class GuardContextBuilder {

    public:

        static const GuardContext build(const BoolExpr guard, const std::vector<Subs> &updates, VariableManager &varMan);

    private:

        struct Split {
            Guard invariant;
            Guard nonInvariant;
        };

        const BoolExpr guard;
        const std::vector<Subs> &updates;
        VariableManager &varMan;

        GuardContextBuilder(const BoolExpr guard, const std::vector<Subs> &updates, VariableManager &varMan);

        const GuardContext build() const;

        const Guard computeConstraints() const;

        const Split splitInvariants(const Guard &constraints) const;

    };

}

#endif //LOAT_STRENGTHENING_GUARD_CONTEXT_BUILDER_H
