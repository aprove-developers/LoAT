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

#include "expr/expression.h"
#include <utility> // for pair

class VariableManager;


// some typedefs for clarity
using LocationIdx = int;
using VariableIdx = int;
using VariablePair = std::pair<VariableIdx, VariableIdx>;


//typedef std::vector<Expression> GuardList;
//typedef std::map<VariableIdx,Expression> UpdateMap;


// TODO: GuardList should probably be an ExpressionSet instead of a vector
// TODO: Unless the performance penalty is too high.
// TODO: Might try to normalize terms before adding them to the guard (e.g. all variables left, only <, <=, ==)
// TODO: To do this, avoid inheriting from ExpressionSet, re-implement the required methods

// GuardList is a list of expressions with some additional methods for convenience
// TODO: rename to Guard
class GuardList : public std::vector<Expression> {
public:
    // inherit constructors of base class
    using std::vector<Expression>::vector;

    void collectVariables(ExprSymbolSet &res) const;
};


// UpdateMap is a map from variables (as indices) to an expression (with which the variable is updated),
// with some additional methods for convenience
// TODO: rename to Update
class UpdateMap : public std::map<VariableIdx,Expression> {
public:
    bool isUpdated(VariableIdx var) const;

    Expression getUpdate(VariableIdx var) const;

    GiNaC::exmap toSubstitution(const VariableManager &varMan) const;
};


// TODO: Add operator<< for GuardList, especially for UpdateMap


#endif // TYPES_H
