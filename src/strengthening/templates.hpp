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

#ifndef LOAT_STRENGTHENING_TEMPLATES_H
#define LOAT_STRENGTHENING_TEMPLATES_H


#include <boost/detail/iterator.hpp>
#include "../expr/expression.hpp"
#include <boost/concept_check.hpp>

class Templates {

public:

    struct Template {

        Template(Expression t, ExprSymbolSet vars, ExprSymbolSet params) :
                t(std::move(t)), vars(std::move(vars)), params(std::move(params)) {}

        const Expression t;
        const ExprSymbolSet vars;
        const ExprSymbolSet params;

    };

    typedef std::vector<Expression>::const_iterator iterator;

    void add(const Template &t);

    const ExprSymbolSet& params() const;

    const ExprSymbolSet& vars() const;

    bool isParametric(const Expression &e) const;

    const std::vector<Expression> subs(const GiNaC::exmap &sigma) const;

    iterator begin() const;

    iterator end() const;

private:

    std::vector<Expression> templates;
    ExprSymbolSet params_;
    ExprSymbolSet vars_;

};


#endif //LOAT_STRENGTHENING_TEMPLATES_H
