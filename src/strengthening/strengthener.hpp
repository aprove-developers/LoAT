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

#ifndef LOAT_STRENGTHENING_STRENGTHENER_H
#define LOAT_STRENGTHENING_STRENGTHENER_H


#include "../its/types.hpp"
#include "../its/rule.hpp"
#include "../its/variablemanager.hpp"
#include "../its/itsproblem.hpp"
#include "types.hpp"
#include "constraintbuilder.hpp"

namespace strengthening {

    class Strengthener {

    public:

        static const option<LinearRule> apply(const LinearRule &r, ITSProblem &its);

    private:

        const RuleContext &ruleCtx;

        explicit Strengthener(const RuleContext &ruleCtx);

        const option<Guard> apply(const Guard &guard) const;

    };

}

#endif //LOAT_STRENGTHENING_STRENGTHENER_H
