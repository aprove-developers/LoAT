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

#include "timing.h"
#include "expression.h"

#include "debug.h"

using namespace std;


/* ############################## *
 * ### Context implementation ### *
 * ############################## */

z3::expr Z3Context::getVariable(string name, VariableType type) {
    auto it = variables.find(name);
    //create new variable
    if (it == variables.end()) {
        z3::expr res = (type == Integer) ? int_const(name.c_str()) : real_const(name.c_str());
        variables.emplace(name,res);
        basenameCount[name] = 1;
        return res;
    }
    //return existing variable if type matches
    if (isTypeEqual(it->second,type))
        return it->second;

    //name already in use for different type
    return getFreshVariable(name,type);
}

z3::expr Z3Context::getFreshVariable(string name, VariableType type, string *newnameptr) {
    //if name is still free, keep it
    if (variables.find(name) == variables.end())
        return getVariable(name,type);

    string newname;
    do {
        int cnt = basenameCount[name]++;
        string suffix = to_string(cnt);
        newname = name + "_" + suffix;
    } while (variables.find(newname) != variables.end());

    if (newnameptr) *newnameptr = newname;

    return getVariable(newname,type);
}

bool Z3Context::hasVariable(string name, VariableType type) const {
    auto it = variables.find(name);
    return it != variables.end() && isTypeEqual(it->second,type);
}

bool Z3Context::hasVariableOfAnyType(string name, VariableType &typeOut) const {
    auto it = variables.find(name);
    if (it == variables.end()) {
        return false;
    } else {
        typeOut = (it->second.get_sort().is_int()) ? Integer : Real;
        return true;
    }
}

bool Z3Context::isTypeEqual(const z3::expr &expr, VariableType type) const {
    const z3::sort sort = expr.get_sort();
    return ((type == Integer && sort.is_int()) || (type == Real && sort.is_real()));
}


