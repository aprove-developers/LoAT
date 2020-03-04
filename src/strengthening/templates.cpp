/*  This file is part of LoAT.
 *  Copyright (c) 2019 Florian Frohn
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

#include "templates.hpp"

typedef Templates Self;

void Self::add(const Self::Template &t) {
    templates.push_back(t.t);
    vars_.insert(t.vars.begin(), t.vars.end());
    params_.insert(t.params.begin(), t.params.end());
}

const ExprSymbolSet& Self::params() const {
    return params_;
}

const ExprSymbolSet& Self::vars() const {
    return vars_;
}

bool Self::isParametric(const Expression &e) const {
    ExprSymbolSet eVars = e.getVariables();
    for (const ExprSymbol &x: params()) {
        if (eVars.count(x) > 0) {
            return true;
        }
    }
    return false;
}

const std::vector<Expression> Self::subs(const ExprMap &sigma) const {
    std::vector<Expression> res;
    for (Expression e: templates) {
        e.applySubs(sigma);
        res.push_back(e);
    }
    return res;
}

Self::iterator Self::begin() const {
    return templates.begin();
}

Self::iterator Self::end() const {
    return templates.end();
}
