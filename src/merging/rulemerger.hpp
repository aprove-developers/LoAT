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

#ifndef LOAT_MERGING_RULEMERGER_H
#define LOAT_MERGING_RULEMERGER_H

#include "../its/itsproblem.hpp"

namespace merging {

    class RuleMerger {

    public:
        static bool mergeRules(ITSProblem &its);

    private:
        RuleMerger(ITSProblem &its);

        bool apply();

        std::vector<std::vector<TransIdx>> findCandidates();

        bool sameEquivalenceClass(const TransIdx &t1, const TransIdx &t2);

        bool merge(const std::vector<TransIdx> &candidates);

        option<Rule> merge(const Rule &r1, const Rule &r2);

        ITSProblem &its;

    };

}

#endif //LOAT_MERGING_RULEMERGER_H
