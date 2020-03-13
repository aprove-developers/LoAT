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
#include "../expr/boolexpr.hpp"

void GuardList::collectVariables(VarSet &res) const {
    for (const Rel &rel : *this) {
        rel.collectVariables(res);
    }
}

GuardList GuardList::subs(const Subs &sigma) const {
    GuardList res;
    for (const Rel &rel: *this) {
        res.push_back(rel.subs(sigma));
    }
    return res;
}

void GuardList::applySubstitution(const Subs &sigma) {
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

std::ostream& operator<<(std::ostream &s, const GuardList &l) {
    return s << buildAnd(l);
}
