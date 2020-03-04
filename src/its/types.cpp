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

#include "types.hpp"
#include "variablemanager.hpp"


void GuardList::collectVariables(ExprSymbolSet &res) const {
    for (const Rel &rel : *this) {
        rel.collectVariables(res);
    }
}

GuardList GuardList::subs(const ExprMap &sigma) const {
    GuardList res;
    for (const Rel &rel: *this) {
        res.push_back(rel.subs(sigma));
    }
    return res;
}

void GuardList::applySubstitution(const ExprMap &sigma) {
    for (Rel &rel: *this) {
        rel.applySubs(sigma);
    }
}

bool UpdateMap::isUpdated(VariableIdx var) const {
    return find(var) != end();
}

Expression UpdateMap::getUpdate(VariableIdx var) const {
    auto it = find(var);
    assert(it != end());
    return it->second;
}

ExprMap UpdateMap::toSubstitution(const VariableManager &varMan) const {
    ExprMap subs;
    for (const auto &it : *this) {
        subs[varMan.getVarSymbol(it.first)] = it.second;
    }
    return subs;
}
