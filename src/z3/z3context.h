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

#include "timing.h"

#include <z3++.h>
#include <map>
#include <boost/optional.hpp>


/**
 * Wrapper around z3 context to allow convenient variable handling
 */
class Z3Context : public z3::context {
public:
    enum VariableType { Integer, Real };

public:
    /**
     * Returns the variable of the given name (and any type), if present
     */
    boost::optional<z3::expr> getVariable(const std::string &name) const;

    /**
     * Like getVariable, but only returns the variable if it has the given type
     */
    boost::optional<z3::expr> getVariable(const std::string &name, VariableType type) const;

    /**
     * Adds a fresh variable to this context and returns it.
     * If possible, the given name is used. If the name is already taken, it is modified by appending a number.
     * The context maps the given name (before modification) to the generated variable.
     */
    z3::expr addFreshVariable(std::string basename, VariableType type = Integer);

private:
    static bool isTypeEqual(const z3::expr &expr, VariableType type);
    std::string generateFreshName(std::string basename);

private:
    // Mapping between numbers and generated variables
    std::map<std::string,z3::expr> variables;

    // Only for efficiency: count how often a name was already given to addFreshVariable.
    // This speeds up generating a fresh name if the same name is requested several times.
    std::map<std::string,int> basenameCount;
};


/**
 * Wrapper around z3 solver to track timing information
 */
class Z3Solver : public z3::solver {
public:
    explicit Z3Solver(Z3Context &context) : z3::solver(context) {}

    inline z3::check_result check() {
        Timing::start(Timing::Z3);
        z3::check_result res = z3::solver::check();
        Timing::done(Timing::Z3);
        return res;
    }
};


#endif // Z3CONTEXT_H
