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

#ifndef LOAT_STRENGTHENING_TEMPLATE_BUILDER_H
#define LOAT_STRENGTHENING_TEMPLATE_BUILDER_H

#include "../../its/variablemanager.hpp"
#include "../../its/rule.hpp"
#include "types.hpp"
#include "../../util/templates.hpp"

namespace strengthening {

    class TemplateBuilder {

    public:

        static const Templates build(const GuardContext &guardCtx, const Rule &rule, VariableManager &varMan);

    private:

        const GuardContext &guardCtx;
        const Rule &rule;
        VariableManager &varMan;

        TemplateBuilder(const GuardContext &guardCtx, const Rule &rule, VariableManager &varMan);

        const Templates build() const;

    };

}

#endif //LOAT_STRENGTHENING_TEMPLATE_BUILDER_H
