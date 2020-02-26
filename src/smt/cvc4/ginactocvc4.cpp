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

#include "ginactocvc4.hpp"

#include "../../config.hpp"
#include "cvc4context.hpp"

using namespace std;
using namespace GiNaC;


GinacToCvc4::GinacToCvc4(Cvc4Context &context, const VariableManager &varMan)
        : context(context), varMan(varMan)
{}

CVC4::Expr GinacToCvc4::convert(const GiNaC::ex &expr, Cvc4Context &context, const VariableManager &varMan) {
    GinacToCvc4 converter(context, varMan);
    CVC4::Expr res = converter.convert_ex(expr);
    return res;
}

CVC4::Expr GinacToCvc4::convert_ex(const GiNaC::ex &e) {
    if (is_a<add>(e)) {
        return convert_add(e);

    } else if (is_a<mul>(e)) {
        return convert_mul(e);

    } else if (is_a<power>(e)) {
        return convert_power(e);

    } else if (is_a<numeric>(e)) {
        return convert_numeric(ex_to<numeric>(e));

    } else if (is_a<symbol>(e)) {
        return convert_symbol(ex_to<symbol>(e));

    } else if (is_a<relational>(e)) {
        return convert_relational(e);
    }

    stringstream ss;
    ss << "Error: GiNaC type not implemented for term: " << e << endl;
    throw GinacCvc4ConversionError(ss.str());
}

CVC4::Expr GinacToCvc4::convert_add(const GiNaC::ex &e) {
    assert(e.nops() > 0);

    CVC4::Expr res = convert_ex(e.op(0));
    for (unsigned int i=1; i < e.nops(); ++i) {
        res = context.mkExpr(CVC4::Kind::PLUS, res, convert_ex(e.op(i)));
    }

    return res;
}

CVC4::Expr GinacToCvc4::convert_mul(const GiNaC::ex &e) {
    assert(e.nops() > 0);

    CVC4::Expr res = convert_ex(e.op(0));
    for (unsigned int i=1; i < e.nops(); ++i) {
        res = context.mkExpr(CVC4::Kind::MULT, res, convert_ex(e.op(i)));
    }

    return res;
}

CVC4::Expr GinacToCvc4::convert_power(const GiNaC::ex &e) {
    assert(e.nops() == 2);

    if (is_a<numeric>(e.op(1))) {
        numeric num = ex_to<numeric>(e.op(1));
        if (num.is_integer() && num.is_positive() && num.to_long() <= Config::Z3::MaxExponentWithoutPow) {
            int exp = num.to_int();
            CVC4::Expr base = convert_ex(e.op(0));

            CVC4::Expr res = base;
            while (--exp > 0) {
                res = context.mkExpr(CVC4::Kind::MULT, res, base);
            }

            return res;
        }
    }

    return context.mkExpr(CVC4::Kind::POW, convert_ex(e.op(0)), convert_ex(e.op(1)));
}

CVC4::Expr GinacToCvc4::convert_numeric(const GiNaC::numeric &num) {
    assert(num.is_integer() || num.is_real());

    try {
        // convert integer either as integer or as reals (depending on settings)
        if (num.is_integer()) {
            return context.mkConst(CVC4::Rational(num.to_long()));
        }

        // always convert real numbers as reals
        return context.mkConst(CVC4::Rational(num.numer().to_long(), num.denom().to_long()));

    } catch (...) {
        throw GinacCvc4LargeConstantError("Numeric constant too large, cannot convert to z3");
    }
}

CVC4::Expr GinacToCvc4::convert_symbol(const GiNaC::symbol &e) {
    // if the symbol is already known, we re-use it (regardless of its type!)
    auto optVar = context.getVariable(e);
    if (optVar) {
        return optVar.get();
    }

    // otherwise we add a fresh z3 variable and associate it with this symbol
    return context.addNewVariable(e, varMan.getType(e));
}

CVC4::Expr GinacToCvc4::convert_relational(const GiNaC::ex &e) {
    assert(e.nops() == 2);

    CVC4::Expr a = convert_ex(e.op(0));
    CVC4::Expr b = convert_ex(e.op(1));

    if (e.info(info_flags::relation_equal)) return context.mkExpr(CVC4::Kind::EQUAL, a, b);
    if (e.info(info_flags::relation_not_equal)) return context.mkExpr(CVC4::Kind::NOT, context.mkExpr(CVC4::Kind::EQUAL, a, b));
    if (e.info(info_flags::relation_less)) return context.mkExpr(CVC4::Kind::LT, a, b);
    if (e.info(info_flags::relation_less_or_equal)) return context.mkExpr(CVC4::Kind::LEQ, a, b);
    if (e.info(info_flags::relation_greater)) return context.mkExpr(CVC4::Kind::GT, a, b);
    if (e.info(info_flags::relation_greater_or_equal)) return context.mkExpr(CVC4::Kind::GEQ, a, b);

    assert(false && "unreachable");
}
