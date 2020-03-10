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


void GuardList::collectVariables(VarSet &res) const {
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

bool GuardList::isWellformed() const {
    for (const Rel &rel : *this) {
        if (rel.isNeq()) return false;
    }
    return true;
}

bool operator<(const GuardList &m1, const GuardList &m2);

bool UpdateMap::isUpdated(VariableIdx var) const {
    return find(var) != end();
}

Expr UpdateMap::getUpdate(VariableIdx var) const {
    auto it = find(var);
    assert(it != end());
    return it->second;
}

ExprMap UpdateMap::toSubstitution(const VariableManager &varMan) const {
    ExprMap subs;
    for (const auto &it : *this) {
        subs.put(varMan.getVarSymbol(it.first), it.second);
    }
    return subs;
}

bool operator==(const UpdateMap &m1, const UpdateMap &m2) {
    if (m1.size() != m2.size()) {
        return false;
    }
    auto it1 = m1.begin();
    auto it2 = m1.begin();
    while (it1 != m1.end() && it2 != m2.end()) {
        if (it1->first != it2->first) return false;
        if (!it1->second.equals(it2->second)) return false;
    }
    return true;
}
