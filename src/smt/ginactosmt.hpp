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

#include <map>
#include <sstream>

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

    static EXPR convert(const Rel &rel, SmtContext<EXPR> &context, const VariableManager &varMan) {
        GinacToSmt<EXPR> converter(context, varMan);
        EXPR res = converter.convert_relational(rel);
        return res;
    }

protected:
    GinacToSmt<EXPR>(SmtContext<EXPR> &context, const VariableManager &varMan): context(context), varMan(varMan) {}

    EXPR convert_ex(const Expression &e){
        if (e.isAdd()) {
            return convert_add(e);

        } else if (e.isMul()) {
            return convert_mul(e);

        } else if (e.isPower()) {
            return convert_power(e);

        } else if (e.isNumeric()) {
            return convert_numeric(e.toNumeric());

        } else if (e.isSymbol()) {
            return convert_symbol(e.toSymbol());

        }

        std::stringstream ss;
        ss << "Error: GiNaC type not implemented for term: " << e << std::endl;
        throw GinacConversionError(ss.str());
    }

    EXPR convert_add(const Expression &e){
        assert(e.nops() > 0);

        EXPR res = convert_ex(e.op(0));
        for (unsigned int i=1; i < e.nops(); ++i) {
            res = context.plus(res, convert_ex(e.op(i)));
        }

        return res;
    }

    EXPR convert_mul(const Expression &e) {
        assert(e.nops() > 0);

        EXPR res = convert_ex(e.op(0));
        for (unsigned int i=1; i < e.nops(); ++i) {
            res = context.times(res, convert_ex(e.op(i)));
        }

        return res;
    }

    EXPR convert_power(const Expression &e) {
        assert(e.nops() == 2);

        //rewrite power as multiplication if possible, which z3 can handle much better (e.g x^3 becomes x*x*x)
        if (e.op(1).isNumeric()) {
            GiNaC::numeric num = e.op(1).toNumeric();
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

    EXPR convert_relational(const Rel &rel) {

        EXPR a = convert_ex(rel.lhs());
        EXPR b = convert_ex(rel.rhs());

        switch (rel.getOp()) {
        case Rel::eq: return context.eq(a, b);
        case Rel::neq: return context.neq(a, b);
        case Rel::lt: return context.lt(a, b);
        case Rel::leq: return context.le(a, b);
        case Rel::gt: return context.gt(a, b);
        case Rel::geq: return context.ge(a, b);
        }

        assert(false && "unreachable");
    }

private:
    SmtContext<EXPR> &context;
    const VariableManager &varMan;
};

#endif // GINACTOSMT_H
