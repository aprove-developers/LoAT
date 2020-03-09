#ifdef HAS_YICES

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

#include "yicescontext.hpp"

using namespace std;

YicesContext::~YicesContext() { }

term_t YicesContext::buildVar(const std::string &name, Expr::Type type) {
    term_t res = (type == Expr::Int) ? yices_new_uninterpreted_term(yices_int_type()) : yices_new_uninterpreted_term(yices_real_type());
    yices_set_term_name(res, name.c_str());
    return res;
}

term_t YicesContext::getInt(long val) {
    return yices_int64(val);
}

term_t YicesContext::getReal(long num, long denom) {
    assert(denom != 0);
    if (denom > 0) {
        return yices_rational64(num, denom);
    } else {
        return yices_rational64(-num, -denom);
    }
}

term_t YicesContext::pow(const term_t &base, const term_t &exp) {
    mpq_t q;
    mpz_t num, denom;
    mpq_init(q);
    int error = yices_rational_const_value(exp, q);
    assert(error == 0);
    mpq_get_num(denom, q);
    assert(mpz_get_si(denom) == 1);
    mpq_get_num(num, q);
    return yices_power(base, mpz_get_ui(num));
}

term_t YicesContext::plus(const term_t &x, const term_t &y) {
    return yices_add(x, y);
}

term_t YicesContext::times(const term_t &x, const term_t &y) {
    return yices_mul(x, y);
}

term_t YicesContext::eq(const term_t &x, const term_t &y) {
    return yices_arith_eq_atom(x, y);
}

term_t YicesContext::lt(const term_t &x, const term_t &y) {
    return yices_arith_lt_atom(x, y);
}

term_t YicesContext::le(const term_t &x, const term_t &y) {
    return yices_arith_leq_atom(x, y);
}

term_t YicesContext::gt(const term_t &x, const term_t &y) {
    return yices_arith_gt_atom(x, y);
}

term_t YicesContext::ge(const term_t &x, const term_t &y) {
    return yices_arith_geq_atom(x, y);
}

term_t YicesContext::neq(const term_t &x, const term_t &y) {
    return yices_arith_neq_atom(x, y);
}

term_t YicesContext::bAnd(const term_t &x, const term_t &y) {
    return yices_and2(x, y);
}

term_t YicesContext::bOr(const term_t &x, const term_t &y) {
    return yices_or2(x, y);
}

term_t YicesContext::bTrue() {
    return yices_true();
}

term_t YicesContext::bFalse() {
    return yices_false();
}

#endif
