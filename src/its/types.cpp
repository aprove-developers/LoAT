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

void Guard::collectVariables(VarSet &res) const {
    for (const Rel &rel : *this) {
        rel.collectVariables(res);
    }
}

Guard Guard::subs(const Subs &sigma) const {
    Guard res;
    for (const Rel &rel: *this) {
        res.push_back(rel.subs(sigma));
    }
    return res;
}

bool Guard::isWellformed() const {
    return std::all_of(begin(), end(), [](const Rel &rel) {
        return !rel.isNeq();
    });
}

bool Guard::isLinear() const {
    return std::all_of(begin(), end(), [](const Rel &rel) {
        return rel.isLinear();
    });
}

bool operator<(const Guard &m1, const Guard &m2);

std::ostream& operator<<(std::ostream &s, const Guard &l) {
    return s << buildAnd(l);
}

Guard operator&(const Guard &fst, const Guard &snd) {
    Guard res(fst);
    res.insert(res.end(), snd.begin(), snd.end());
    return res;
}

Guard operator&(const Guard &fst, const Rel &snd) {
    Guard res(fst);
    res.push_back(snd);
    return res;
}
