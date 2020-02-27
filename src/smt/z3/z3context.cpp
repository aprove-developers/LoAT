#ifdef HAS_Z3

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

#include "z3context.hpp"

using namespace std;

Z3Context::~Z3Context() { }

z3::expr Z3Context::buildVar(const std::string &name, Expression::Type type) {
    return (type == Expression::Int) ? int_const(name.c_str()) : real_const(name.c_str());
}

z3::expr Z3Context::getInt(long val) {
    return int_val(val);
}

z3::expr Z3Context::getReal(long num, long denom) {
    return real_val(num, denom);
}

z3::expr Z3Context::pow(const z3::expr &base, const z3::expr &exp) {
    return z3::pw(base, exp);
}

z3::expr Z3Context::plus(const z3::expr &x, const z3::expr &y) {
    return x + y;
}

z3::expr Z3Context::times(const z3::expr &x, const z3::expr &y) {
    return x * y;
}

z3::expr Z3Context::eq(const z3::expr &x, const z3::expr &y) {
    return x == y;
}

z3::expr Z3Context::lt(const z3::expr &x, const z3::expr &y) {
    return x < y;
}

z3::expr Z3Context::le(const z3::expr &x, const z3::expr &y) {
    return x <= y;
}

z3::expr Z3Context::gt(const z3::expr &x, const z3::expr &y) {
    return x > y;
}

z3::expr Z3Context::ge(const z3::expr &x, const z3::expr &y) {
    return x >= y;
}

z3::expr Z3Context::neq(const z3::expr &x, const z3::expr &y) {
    return x != y;
}

#endif
