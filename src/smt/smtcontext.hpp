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

#ifndef SMTCONTEXT_H
#define SMTCONTEXT_H

#include "../util/option.hpp"
#include "../expr/expression.hpp"

#include <map>

template<class EXPR>
class SmtContext {

public:

    virtual EXPR getInt(long val) = 0;
    virtual EXPR getReal(long num, long denom) = 0;
    virtual EXPR pow(const EXPR &base, const EXPR &exp) = 0;
    virtual EXPR plus(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR times(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR eq(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR lt(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR le(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR gt(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR ge(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR neq(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR bAnd(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR bOr(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR bTrue() = 0;
    virtual EXPR bFalse() = 0;

    option<EXPR> getVariable(const Var &symbol) const {
        auto it = symbolMap.find(symbol);
        if (it != symbolMap.end()) {
            return it->second;
        }
        return {};
    }

    EXPR addNewVariable(const Var &symbol, Expr::Type type = Expr::Int) {
        assert(symbolMap.count(symbol) == 0);
        EXPR res = generateFreshVar(symbol.get_name(), type);
        symbolMap.emplace(symbol, res);
        return res;
    }

    VarMap<EXPR> getSymbolMap() const {
        return symbolMap;
    }

    virtual ~SmtContext() {}

protected:

    EXPR generateFreshVar(const std::string &basename, Expr::Type type) {
        std::string newname = basename;

        while (usedNames.find(newname) != usedNames.end()) {
            int cnt = usedNames[basename]++;
            newname = basename + "_" + std::to_string(cnt);
        }

        usedNames.emplace(newname, 1); // newname is now used once
        return buildVar(newname, type);
    }

    virtual EXPR buildVar(const std::string &basename, Expr::Type type) = 0;

protected:
    VarMap<EXPR> symbolMap;
    std::map<std::string,int> usedNames;
};

#endif // SMTCONTEXT_H
