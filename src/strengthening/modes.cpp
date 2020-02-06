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

    const std::vector<Mode> Self::modes() {
        return {invariance};
    }

    const MaxSmtConstraints Self::invariance(const SmtConstraints &constraints, bool preferInvariance, Z3Context &z3Ctx) {
        MaxSmtConstraints res;
        res.soft.insert(res.soft.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
        if (preferInvariance) {
            z3::expr_vector allInvariant(z3Ctx);
            for (const z3::expr &e: constraints.conclusionsInvariant) {
                allInvariant.push_back(e);
                res.soft.push_back(z3::mk_and(allInvariant));
            }
        }
        z3::expr_vector sat(z3Ctx);
        for (const z3::expr &e: constraints.initiation.satisfiable) {
            sat.push_back(e);
        }
        res.hard.push_back(z3::mk_or(sat));
        z3::expr_vector someMonotonic(z3Ctx);
        for (const z3::expr &e: constraints.conclusionsMonotonic) {
            res.soft.push_back(e);
            someMonotonic.push_back(e);
        }
        res.hard.push_back(z3::mk_or(someMonotonic));
        res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
        return res;
    }

    // const MaxSmtConstraints Self::monotonicity(const SmtConstraints &constraints, Z3Context &z3Ctx) {
    //     MaxSmtConstraints res;
    //     res.soft.insert(res.soft.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
    //     z3::expr_vector satisfiableForSomePredecessor(z3Ctx);
    //     for (const z3::expr &e: constraints.initiation.satisfiable) {
    //         satisfiableForSomePredecessor.push_back(e);
    //     }
    //     res.hard.push_back(z3::mk_or(satisfiableForSomePredecessor));
    //     z3::expr_vector someConclusionMonotonic(z3Ctx);
    //     for (const z3::expr &e: constraints.conclusionsMonotonic) {
    //         res.soft.push_back(e);
    //         someConclusionMonotonic.push_back(e);
    //     }
    //     for (const z3::expr &e: constraints.conclusionsInvariant) {
    //         someConclusionMonotonic.push_back(e);
    //     }
    //     res.hard.push_back(z3::mk_or(someConclusionMonotonic));
    //     res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
    //     return res;
    // }

}
