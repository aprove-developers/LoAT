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

#include "z3solver.hpp"

using namespace std;

option<z3::model> Z3Solver::maxSmt(std::vector<z3::expr> hard, std::vector<z3::expr> soft) {
    for (const z3::expr &e: hard) {
        this->add(e);
    }
    if (this->check() != z3::check_result::sat) {
        return {};
    }
    z3::model model = this->get_model();
    for (const z3::expr &e: soft) {
        if (Timeout::soft()) {
            return {};
        }
        this->push();
        this->add(e);
        if (this->check() == z3::check_result::sat) {
            model = this->get_model();
        } else {
            this->pop();
        }
    }
    return model;
}

void Z3Solver::setTimeout(Z3Context &context, unsigned int timeout) {
    if (this->timeout && this->timeout.get() == timeout) {
        return;
    } else if (timeout > 0) {
        z3::params params(context);
        params.set(":timeout", timeout);
        this->set(params);
        this->timeout = {timeout};
    }
}
