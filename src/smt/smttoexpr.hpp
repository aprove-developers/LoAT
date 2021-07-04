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

#ifndef SMTTOEXPR_H
#define SMTTOEXPR_H

#include "../util/exceptions.hpp"
#include "smtcontext.hpp"
#include "../its/itsproblem.hpp"
#include "../config.hpp"
#include "../expr/boolexpr.hpp"

#include <map>
#include <sstream>

template<typename EXPR> class SmtToExpr {
public:

    static BoolExpr convert(const EXPR &e, SmtContext<EXPR> &ctx) {
        if (ctx.isTrue(e)) return True;
        if (ctx.isFalse(e)) return False;
        if (ctx.isNot(e)) return !convert(ctx.getChildren(e)[0], ctx);
        if (ctx.isLit(e)) {
            SmtToExpr<EXPR> converter(ctx);
            BoolExpr res = buildLit(converter.convert_relational(e));
            return res;
        }
        BoolExprSet children;
        for (const EXPR &c: ctx.getChildren(e)) {
            children.insert(convert(c, ctx));
        }
        return ctx.isAnd(e) ? buildAnd(children) : buildOr(children);
    }

protected:
    SmtToExpr<EXPR>(SmtContext<EXPR> &context): context(context) {}

    Expr convert_ex(const EXPR &e){
        if (context.isNoOp(e)) {
            const std::vector<EXPR> children = context.getChildren(e);
            assert(children.size() == 1);
            return convert_ex(children[0]);
        } else if (context.isAdd(e)) {
            return convert_add(e);
        } else if (context.isMul(e)) {
            return convert_mul(e);
        } else if (context.isDiv(e)) {
            return convert_mul(e);
        } else if (context.isPow(e)) {
            return convert_power(e);
        } else if (context.isRationalConstant(e)) {
            return convert_numeric(e);
        } else if (context.isVar(e)) {
            return convert_symbol(e);
        }
        context.printStderr(e);
        throw std::invalid_argument("unknown operator");
    }

    Expr convert_add(const EXPR &e){
        const std::vector<EXPR> &children = context.getChildren(e);
        assert(!children.empty());
        option<Expr> res;
        for (const EXPR &c: children) {
            if (!res) {
                res = convert_ex(c);
            } else {
                res = res.get() + convert_ex(c);
            }
        }

        return res.get();
    }

    Expr convert_mul(const EXPR &e) {
        const std::vector<EXPR> &children = context.getChildren(e);
        assert(!children.empty());
        option<Expr> res;
        for (const EXPR &c: children) {
            if (!res) {
                res = convert_ex(c);
            } else {
                res = res.get() * convert_ex(c);
            }
        }

        return res.get();
    }

    Expr convert_div(const EXPR &e) {
        const std::vector<EXPR> &children = context.getChildren(e);
        assert(children.size() == 2);
        assert(context.isRationalConstant(children[0]));
        assert(context.isRationalConstant(children[1]));
        const Expr &x = convert_numeric(context.isRationalConstant(children[0]));
        const Expr &y = convert_numeric(context.isRationalConstant(children[1]));
        return x.toNum() / y.toNum();
    }

    Expr convert_power(const EXPR &e) {
        const std::vector<EXPR> &children = context.getChildren(e);
        assert(children.size() == 2);
        return convert_ex(children[0]) ^ convert_ex(children[1]);
    }

    Expr convert_numeric(const EXPR &num) {
        if (context.isInt(num)) {
            return Expr(context.toInt(num));
        }
        return Expr(context.numerator(num)) / context.denominator(num);
    }

    Var convert_symbol(const EXPR &e) {
        // if the symbol is already known, we re-use it (regardless of its type!)
        auto optVar = context.getVariable(context.getName(e));
        if (optVar) {
            return optVar.get();
        }
        throw std::invalid_argument("unknown variable");
    }

    Rel convert_relational(const EXPR &rel) {
        Expr a = convert_ex(context.lhs(rel));
        Expr b = convert_ex(context.rhs(rel));
        return Rel(a, context.relOp(rel), b);
    }

private:
    SmtContext<EXPR> &context;
};

#endif
