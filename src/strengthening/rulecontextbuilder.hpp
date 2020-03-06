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

#ifndef LOAT_STRENGTHENING_RULE_CONTEXT_BUILDER_H
#define LOAT_STRENGTHENING_RULE_CONTEXT_BUILDER_H

#include "../its/rule.hpp"
#include "../its/itsproblem.hpp"
#include "types.hpp"

namespace strengthening {

    class RuleContextBuilder {

    public:

        static const RuleContext build(const Rule &rule, ITSProblem &its);

    private:

        const Rule &rule;
        ITSProblem &its;

        RuleContextBuilder(const Rule &rule, ITSProblem &its);

        const RuleContext build() const;

        const std::vector<ExprMap> computeUpdates() const;

    };

}

#endif //LOAT_STRENGTHENING_RULE_CONTEXT_BUILDER_H
