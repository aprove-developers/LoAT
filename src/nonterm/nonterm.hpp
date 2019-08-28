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

#ifndef LOAT_NONTERM_H
#define LOAT_NONTERM_H

#include "../its/rule.hpp"
#include "../its/itsproblem.hpp"
#include "../accelerate/forward.hpp"

namespace nonterm {

    class NonTerm {

    public:

        static option<std::pair<Rule, ForwardAcceleration::ResultKind>> apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink);

    private:

        static option<Rule> checkRecurrentSet(const Rule &r, const ITSProblem &its, const LocationIdx &sink);

        static option<Rule> checkEventualRecurrentSet(const Rule &r, const ITSProblem &its, const LocationIdx &sink);

        static option<Rule> searchFixpoint(const Rule &r, const ITSProblem &its, const LocationIdx &sink);

    };

}


#endif //LOAT_NONTERM_H
