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

        Template(Rel t, VarSet vars, VarSet params) :
                t(std::move(t)), vars(std::move(vars)), params(std::move(params)) {}

        const Rel t;
        const VarSet vars;
        const VarSet params;

    };

    typedef std::vector<Rel>::const_iterator iterator;

    void add(const Template &t);

    const VarSet& params() const;

    const VarSet& vars() const;

    bool isParametric(const Rel &rel) const;

    const std::vector<Rel> subs(const ExprMap &sigma) const;

    iterator begin() const;

    iterator end() const;

private:

    std::vector<Rel> templates;
    VarSet params_;
    VarSet vars_;

};


#endif //LOAT_STRENGTHENING_TEMPLATES_H
