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

#include "../its/rule.hpp"
#include "../its/itsproblem.hpp"
#include "../expr/guardtoolbox.hpp"
#include "rulecontextbuilder.hpp"
#include "strengthener.hpp"

namespace strengthening {

    typedef RuleContextBuilder Self;

    const RuleContext Self::build(const Rule &rule, ITSProblem &its) {
        return RuleContextBuilder(rule, its).build();
    }

    Self::RuleContextBuilder(const Rule &rule, ITSProblem &its): rule(rule), its(its) { }

    const std::vector<Subs> Self::computeUpdates() const {
        std::vector<Subs> res;
        for (const RuleRhs &rhs: rule.getRhss()) {
            res.push_back(rhs.getUpdate().toSubstitution(its));
        }
        return res;
    }

    const RuleContext Self::build() const {
        const std::vector<Subs> &updates = computeUpdates();
        return RuleContext(rule, updates, its);
    }

}
