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

#include "../util/exceptions.hpp"
#include "../z3/z3context.hpp"

#include <ginac/ginac.h>
#include <z3++.h>
#include <map>


class GinacToZ3 {
public:
    EXCEPTION(GinacZ3ConversionError,CustomException);
    EXCEPTION(GinacZ3LargeConstantError,CustomException);

    /**
     * Converts this expression from a GiNaC::ex (or Expression) to a Z3 expression, using the given z3 context.
     * May throw GinacZ3ConversionError (unknown type of GiNaC::ex) or GinacZ3LargeConstantError (constant too large).
     *
     * @param useReals If true, all NEWLY created variables and constants are encoded as reals.
     * Variables already present in the Z3Context are re-used (even if they are integer variables).
     * If false, all newly created variables and constants are integers, except for real constants like 1/2.
     */
    static z3::expr convert(const GiNaC::ex &expr, Z3Context &context);

private:
    GinacToZ3(Z3Context &context);

    z3::expr convert_ex(const GiNaC::ex &e);
    z3::expr convert_add(const GiNaC::ex &e);
    z3::expr convert_mul(const GiNaC::ex &e);
    z3::expr convert_power(const GiNaC::ex &e);
    z3::expr convert_numeric(const GiNaC::numeric &num);
    z3::expr convert_symbol(const GiNaC::symbol &sym);
    z3::expr convert_relational(const GiNaC::ex &e);

    // returns the variable type for fresh variables according to settings
    Z3Context::VariableType variableType() const;

private:
    Z3Context &context;
};

#endif // GINACTOZ3_H
