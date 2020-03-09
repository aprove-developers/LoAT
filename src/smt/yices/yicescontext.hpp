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

#ifndef YICESCONTEXT_H
#define YICESCONTEXT_H

#include "../smtcontext.hpp"
#include "../../util/option.hpp"
#include "../../expr/expression.hpp"

#include <gmp.h>
#include <yices.h>
#include <map>


class YicesContext : public SmtContext<term_t> {

public:
    ~YicesContext() override;
    term_t getInt(long val) override;
    term_t getReal(long num, long denom) override;
    term_t pow(const term_t &base, const term_t &exp) override;
    term_t plus(const term_t &x, const term_t &y) override;
    term_t times(const term_t &x, const term_t &y) override;
    term_t eq(const term_t &x, const term_t &y) override;
    term_t lt(const term_t &x, const term_t &y) override;
    term_t le(const term_t &x, const term_t &y) override;
    term_t gt(const term_t &x, const term_t &y) override;
    term_t ge(const term_t &x, const term_t &y) override;
    term_t neq(const term_t &x, const term_t &y) override;
    term_t bAnd(const term_t &x, const term_t &y) override;
    term_t bOr(const term_t &x, const term_t &y) override;
    term_t bTrue() override;
    term_t bFalse() override;

protected:
    term_t buildVar(const std::string &basename, Expr::Type type) override;

};

#endif // YICESCONTEXT_H

#endif
