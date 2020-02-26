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

#ifndef GINACTOCVC4_H
#define GINACTOCVC4_H

#include "../../util/exceptions.hpp"
#include "cvc4context.hpp"
#include "../../its/itsproblem.hpp"

#include <ginac/ginac.h>
#include <map>


class GinacToCvc4 {
public:
    EXCEPTION(GinacCvc4ConversionError,CustomException);
    EXCEPTION(GinacCvc4LargeConstantError,CustomException);

    static CVC4::Expr convert(const GiNaC::ex &expr, Cvc4Context &context, const VariableManager &varMan);

private:
    GinacToCvc4(Cvc4Context &context, const VariableManager &varMan);

    CVC4::Expr convert_ex(const GiNaC::ex &e);
    CVC4::Expr convert_add(const GiNaC::ex &e);
    CVC4::Expr convert_mul(const GiNaC::ex &e);
    CVC4::Expr convert_power(const GiNaC::ex &e);
    CVC4::Expr convert_numeric(const GiNaC::numeric &num);
    CVC4::Expr convert_symbol(const GiNaC::symbol &sym);
    CVC4::Expr convert_relational(const GiNaC::ex &e);

private:
    Cvc4Context &context;
    const VariableManager &varMan;
};

#endif // GINACTOCVC4_H
