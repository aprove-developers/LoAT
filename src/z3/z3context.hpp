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

#ifndef Z3CONTEXT_H
#define Z3CONTEXT_H

#include "../util/option.hpp"
#include "../expr/expression.hpp"

#include <z3++.h>
#include <map>


/**
 * Wrapper around z3 context to allow convenient variable handling.
 *
 * Note that z3 identifies symbols with the same name, whereas GiNaC considers two symbols with the same name
 * as different. This context does thus map GiNaC symbols to z3 symbols (instead of mapping names to z3 symbols).
 *
 * For convenience, it is also possible to create z3 symbols not associated to any GiNaC symbol,
 * but these symbols cannot be looked up later (as they are not associated to any GiNaC symbol).
 */
class Z3Context : public z3::context {
public:
    enum VariableType { Integer, Real };

public:
    /**
     * Returns the variable associated with the given symbol, if present
     */
    option<z3::expr> getVariable(const ExprSymbol &symbol) const;

    /**
     * Adds a new z3 variable (with the given symbol's name, if possible, otherwise a number is appended)
     * and associates it to the given GiNaC symbol.
     *
     * @note This method must not be called twice for the same GiNaC symbol
     * (i.e., each GiNaC symbol can only be associated to a single z3 variable).
     */
    z3::expr addNewVariable(const ExprSymbol &symbol, VariableType type = Integer);

    /**
     * Adds a new z3 variable (with the given name, if possible, otherwise a number is appended).
     * The new variable is not associated to any GiNaC symbol, hence lookup via getVariable is not possible!
     * This is provided for convenience, one could also use addNewVariable with a newly created GiNaC symbol.
     */
    z3::expr addFreshVariable(const std::string &basename, VariableType type = Integer);

    /**
     * Static wrapper around z3::expr::sort that checks if the given symbol is of the given type.
     * @note symbol must be a z3::symbol, not an arbitrary z3::expr!
     */
    static bool isVariableOfType(const z3::expr &symbol, VariableType type);

    ExprSymbolMap<z3::expr> getSymbolMap() const;

private:
    // Generates a z3 variable of the given type with a fresh name based on the given name
    z3::expr generateFreshVar(const std::string &basename, VariableType type);

private:
    // Maps GiNaC symbols to their associated z3 symbols/variables.
    // Only used for lookup via getVariable.
    ExprSymbolMap<z3::expr> symbolMap;

    // The set of names used by the generated z3 variables (used to find fresh names).
    // Only for efficiency: Count how often a name was already given to generateFreshName.
    // (this speeds up generating a fresh name if the same name is requested several times).
    std::map<std::string,int> usedNames;
};


/**
 * For debugging
 */
std::ostream& operator<<(std::ostream &s, const Z3Context::VariableType &type);


#endif // Z3CONTEXT_H
