/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
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

#ifndef Z3SOLVER_H
#define Z3SOLVER_H

#include "z3context.hpp"
#include "../config.hpp"

#include <z3++.h>


/**
 * Wrapper around z3 solver to track timing information and set timeouts
 */
class Z3Solver : public z3::solver {
public:
    // Constructs a new solver with the given timeout, pass 0 to disable timeout
    explicit Z3Solver(Z3Context &context, unsigned int timeout = Config::Z3::DefaultTimeout) : z3::solver(context), ctx(context) {
        setTimeout(context, timeout);
    }

    inline z3::check_result check() {
        z3::check_result res = z3::solver::check();
        return res;
    }

    inline void add(const z3::expr &e) {
        z3::solver::add(e);
    }

    void setTimeout(Z3Context &context, unsigned int timeout);

    option<z3::model> maxSmt(std::vector<z3::expr> hard, std::vector<z3::expr> soft);

private:
    Z3Context &ctx;
    option<unsigned int> timeout;

};


#endif // Z3SOLVER_H
