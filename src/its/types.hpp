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

#ifndef TYPES_H
#define TYPES_H

#include "../expr/expression.hpp"
#include <boost/utility.hpp> // for pair

class VariableManager;


// some typedefs for clarity
using TransIdx = unsigned int;
using LocationIdx = unsigned int;


// GuardList is a list of expressions with some additional methods for convenience
class GuardList : public std::vector<Rel> {
public:
    // inherit constructors of base class
    using std::vector<Rel>::vector;
    void collectVariables(VarSet &res) const;
    GuardList subs(const Subs &sigma) const;
    void applySubstitution(const Subs &sigma);

    /**
     * Returns true iff all guard terms are relational without the use of !=
     */
    bool isWellformed() const;

    friend GuardList operator&(const GuardList &fst, const GuardList &snd);
    friend GuardList operator&(const GuardList &fst, const Rel &snd);

};

std::ostream& operator<<(std::ostream &s, const GuardList &l);

#endif // TYPES_H
