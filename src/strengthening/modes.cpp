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

#include "modes.hpp"
#include "../z3/z3context.hpp"

namespace strengthening {

    typedef Modes Self;

    const MaxSmtConstraints Self::invariance(const SmtConstraints &constraints, Z3Context &z3Ctx) {
        MaxSmtConstraints res;
        res.soft.insert(res.soft.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
        z3::expr_vector sat(z3Ctx);
        for (const z3::expr &e: constraints.initiation.satisfiable) {
            sat.push_back(e);
        }
        res.hard.push_back(z3::mk_or(sat));
        z3::expr_vector someInvariant(z3Ctx);
        for (const z3::expr &e: constraints.conclusionsInvariant) {
            res.hard.push_back(e);
        }
        res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
        return res;
    }

}
