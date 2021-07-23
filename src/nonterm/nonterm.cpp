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

#include "nonterm.hpp"
#include "../accelerate/meter/metertools.hpp"
#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"
#include "../accelerate/recurrence/dependencyorder.hpp"
#include "../analysis/chain.hpp"
#include "../util/relevantvariables.hpp"
#include "../util/status.hpp"

namespace nonterm {

    option<std::pair<Rule, Proof>> NonTerm::universal(const Rule &r, ITSProblem &its, const LocationIdx &sink) {
        if (!Smt::isImplication(r.getGuard(), buildLit(r.getCost() > 0), its)) {
            return {};
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const Subs &up = r.getUpdate(i);
            if (Smt::isImplication(r.getGuard(), r.getGuard()->subs(up), its)) {
                Rule nontermRule(r.getLhsLoc(), r.getGuard(), Expr::NontermSymbol, sink, {});
                Proof proof;
                proof.ruleTransformationProof(r, "non-termination processor", nontermRule, its);
                return {{nontermRule, proof}};
            }
        }
        if (r.isLinear()) {
            Rule chained = Chaining::chainRules(its, r, r, false).get();
            const Subs &up = chained.getUpdate(0);
            if (Smt::check(chained.getGuard(), its) == Smt::Sat && Smt::isImplication(chained.getGuard(), chained.getGuard()->subs(up), its)) {
                Rule nontermRule(chained.getLhsLoc(), chained.getGuard(), Expr::NontermSymbol, sink, {});
                Proof proof;
                proof.ruleTransformationProof(r, "unrolling", chained, its);
                proof.ruleTransformationProof(chained, "non-termination processor", nontermRule, its);
                return {{nontermRule, proof}};
            }
        }
        return {};
    }

}
