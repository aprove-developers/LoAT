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

#include "z3context.h"

#include "expr/expression.h"

using namespace std;
using boost::optional;


optional<z3::expr> Z3Context::getVariable(const std::string &name) const {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return it->second;
    }
    return {};
}

optional<z3::expr> Z3Context::getVariable(const std::string &name, Z3Context::VariableType type) const {
    auto it = variables.find(name);
    if (it != variables.end()) {
        if (isTypeEqual(it->second,type)) {
            return it->second;
        }
    }
    return {};
}

z3::expr Z3Context::addFreshVariable(std::string basename, Z3Context::VariableType type) {
    string name = generateFreshName(basename);
    z3::expr res = (type == Integer) ? int_const(name.c_str()) : real_const(name.c_str());
    variables.emplace(name,res);
    return res;
}

std::string Z3Context::generateFreshName(std::string basename) {
    string newname;

    do {
        int cnt = basenameCount[basename]++;
        newname = basename + "_" + to_string(cnt);
    } while (variables.find(newname) != variables.end());

    return newname;
}

bool Z3Context::isTypeEqual(const z3::expr &expr, VariableType type) {
    const z3::sort sort = expr.get_sort();
    return ((type == Integer && sort.is_int()) || (type == Real && sort.is_real()));
}

