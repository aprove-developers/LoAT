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

#ifndef GINACTOZ3_H
#define GINACTOZ3_H

#include "global.h"
#include "exceptions.h"
#include "z3/z3context.h"

#include <ginac/ginac.h>
#include <z3++.h>
#include <map>


class GinacToZ3 {
public:
    EXCEPTION(GinacZ3ConversionError,CustomException);
    EXCEPTION(GinacZ3LargeConstantError,CustomException);

    struct Settings {
        /**
         * Whether to re-use variables from the context or use a fresh set of variables.
         * If false, existing variables from the context are re-used.
         * If true, one fresh variable is added to the context for each symbol occurring in this expression
         * (multiple occurrences of a symbol all use the same fresh variable).
         *
         * @note: if a variable is re-used from the context, its type is kept (regardless of useReals)
         */
        bool freshVars = false;

        /**
         * Whether variables and constants should be of type real.
         * If true, all NEWLY created variables and constants are encoded as reals.
         * If false, all variables and constants are integers, except for real constants like 1/2.
         *
         * @note: if a variable is re-used from the context, its type is kept (regardless of this setting)
         */
        bool useReals = false;

        // Does not compile without explicitly specifying this ctor (yay C++)
        Settings() {}
    };

    /**
     * Converts this expression from a GiNaC::ex (or Expression) to a Z3 expression, using the given z3 context.
     * May throw GinacZ3ConversionError (unknown type of GiNaC::ex) or GinacZ3LargeConstantError (constant too large).
     */
    static z3::expr convert(const GiNaC::ex &expr, Z3Context &context, Settings cfg = {});

private:
    GinacToZ3(Z3Context &context, Settings cfg);

    z3::expr convert_ex(const GiNaC::ex &e);
    z3::expr convert_add(const GiNaC::ex &e);
    z3::expr convert_mul(const GiNaC::ex &e);
    z3::expr convert_power(const GiNaC::ex &e);
    z3::expr convert_numeric(const GiNaC::numeric &num);
    z3::expr convert_symbol(const GiNaC::symbol &sym);
    z3::expr convert_relational(const GiNaC::ex &e);

    // lookup in freshVariables or create a fresh variable via the context
    z3::expr getFreshVar(const std::string &name);

    // returns the variable type for fresh variables according to settings
    Z3Context::VariableType variableType() const;

private:
    std::map<std::string, z3::expr> freshVariables;
    Z3Context &context;
    Settings settings;
};

#endif // GINACTOZ3_H
