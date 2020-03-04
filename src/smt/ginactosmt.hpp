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

#ifndef GINACTOSMT_H
#define GINACTOSMT_H

#include "../util/exceptions.hpp"
#include "smtcontext.hpp"
#include "../its/itsproblem.hpp"
#include "../config.hpp"
#include "../expr/boolexpr.hpp"

#include <ginac/ginac.h>
#include <map>
#include <sstream>

using namespace GiNaC;

template<typename EXPR> class GinacToSmt {
public:
    EXCEPTION(GinacConversionError,CustomException);
    EXCEPTION(GinacLargeConstantError,CustomException);

    static EXPR convert(const BoolExpr &e, SmtContext<EXPR> &ctx, const VariableManager &varMan) {
        if (e->getLit()) {
            return convert(e->getLit().get(), ctx, varMan);
        }
        EXPR res = ctx.bTrue();
        bool first = true;
        for (const BoolExpr &c: e->getChildren()) {
            if (first) {
                res = convert(c, ctx, varMan);
                first = false;
            } else {
                res = e->isAnd() ? ctx.bAnd(res, convert(c, ctx, varMan)) : ctx.bOr(res, convert(c, ctx, varMan));
            }
        }
        return res;
    }

    static EXPR convert(const GiNaC::ex &expr, SmtContext<EXPR> &context, const VariableManager &varMan) {
        GinacToSmt<EXPR> converter(context, varMan);
        EXPR res = converter.convert_ex(expr);
        return res;
    }

protected:
    GinacToSmt<EXPR>(SmtContext<EXPR> &context, const VariableManager &varMan): context(context), varMan(varMan) {}

    EXPR convert_ex(const GiNaC::ex &e){
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

        std::stringstream ss;
        ss << "Error: GiNaC type not implemented for term: " << e << std::endl;
        throw GinacConversionError(ss.str());
    }

    EXPR convert_add(const GiNaC::ex &e){
        assert(e.nops() > 0);

        EXPR res = convert_ex(e.op(0));
        for (unsigned int i=1; i < e.nops(); ++i) {
            res = context.plus(res, convert_ex(e.op(i)));
        }

        return res;
    }

    EXPR convert_mul(const GiNaC::ex &e) {
        assert(e.nops() > 0);

        EXPR res = convert_ex(e.op(0));
        for (unsigned int i=1; i < e.nops(); ++i) {
            res = context.times(res, convert_ex(e.op(i)));
        }

        return res;
    }

    EXPR convert_power(const GiNaC::ex &e) {
        assert(e.nops() == 2);

        //rewrite power as multiplication if possible, which z3 can handle much better (e.g x^3 becomes x*x*x)
        if (is_a<numeric>(e.op(1))) {
            numeric num = ex_to<numeric>(e.op(1));
            if (num.is_integer() && num.is_positive() && num.to_long() <= Config::Z3::MaxExponentWithoutPow) {
                int exp = num.to_int();
                EXPR base = convert_ex(e.op(0));

                EXPR res = base;
                while (--exp > 0) {
                    res = context.times(res, base);
                }

                return res;
            }
        }

        //use z3 power as fallback (only poorly supported)
        return context.pow(convert_ex(e.op(0)), convert_ex(e.op(1)));
    }

    EXPR convert_numeric(const GiNaC::numeric &num) {
        assert(num.is_integer() || num.is_real());

        try {
            // convert integer either as integer or as reals (depending on settings)
            if (num.is_integer()) {
                return context.getInt(num.to_long());
            }

            // always convert real numbers as reals
            return context.getReal(num.numer().to_long(), num.denom().to_long());

        } catch (...) {
            throw GinacLargeConstantError("Numeric constant too large, cannot convert to z3");
        }
    }

    EXPR convert_symbol(const GiNaC::symbol &e) {
        // if the symbol is already known, we re-use it (regardless of its type!)
        auto optVar = context.getVariable(e);
        if (optVar) {
            return optVar.get();
        }

        // otherwise we add a fresh z3 variable and associate it with this symbol
        return context.addNewVariable(e, varMan.getType(e));
    }

    EXPR convert_relational(const GiNaC::ex &e) {
        assert(e.nops() == 2);

        EXPR a = convert_ex(e.op(0));
        EXPR b = convert_ex(e.op(1));

        if (e.info(info_flags::relation_equal)) return context.eq(a, b);
        if (e.info(info_flags::relation_not_equal)) return context.neq(a, b);
        if (e.info(info_flags::relation_less)) return context.lt(a, b);
        if (e.info(info_flags::relation_less_or_equal)) return context.le(a, b);
        if (e.info(info_flags::relation_greater)) return context.gt(a, b);
        if (e.info(info_flags::relation_greater_or_equal)) return context.ge(a, b);

        assert(false && "unreachable");
    }

private:
    SmtContext<EXPR> &context;
    const VariableManager &varMan;
};

#endif // GINACTOSMT_H