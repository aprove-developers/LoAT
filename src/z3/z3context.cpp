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

#include "z3context.hpp"

#include "../expr/expression.hpp"

using namespace std;


option<z3::expr> Z3Context::getVariable(const ExprSymbol &symbol) const {
    auto it = symbolMap.find(symbol);
    if (it != symbolMap.end()) {
        return it->second;
    }
    return {};
}

z3::expr Z3Context::addNewVariable(const ExprSymbol &symbol, Expression::Type type) {
    // This symbol must not have been mapped to a z3 variable before (can be checked via getVariable)
    assert(symbolMap.count(symbol) == 0);

    // Associated the GiNaC symbol with the resulting variable
    z3::expr res = generateFreshVar(symbol.get_name(), type);
    symbolMap.emplace(symbol, res);
    return res;
}

z3::expr Z3Context::addFreshVariable(const std::string &basename, Expression::Type type) {
    // Generate a fresh variable, but do not associate it to anything
    return generateFreshVar(basename, type);
}

z3::expr Z3Context::generateFreshVar(const std::string &basename, Expression::Type type) {
    string newname = basename;

    while (usedNames.find(newname) != usedNames.end()) {
        int cnt = usedNames[basename]++;
        newname = basename + "_" + to_string(cnt);
    }

    usedNames.emplace(newname, 1); // newname is now used once
    return (type == Expression::Int) ? int_const(newname.c_str()) : real_const(newname.c_str());
}

bool Z3Context::isVariableOfType(const z3::expr &symbol, Expression::Type type) {
    const z3::sort sort = symbol.get_sort();
    return ((type == Expression::Int && sort.is_int()) || (type == Expression::Real && sort.is_real()));
}

std::ostream& operator<<(std::ostream &s, const Expression::Type &type) {
    switch (type) {
        case Expression::Real: s << "Real"; break;
        case Expression::Int: s << "Integer"; break;
    }
    return s;
}

ExprSymbolMap<z3::expr> Z3Context::getSymbolMap() const {
    return symbolMap;
}
