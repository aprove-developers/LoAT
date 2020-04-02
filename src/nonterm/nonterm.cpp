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

    option<std::pair<Rule, Proof>> NonTerm::universal(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
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

    option<std::pair<Rule, Proof>> NonTerm::fixedPoint(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        if (!Smt::isImplication(r.getGuard(), buildLit(r.getCost() > 0), its)) {
            return {};
        }
        std::unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic({r.getGuard()}, r.getUpdates()), its);
        solver->add(r.getGuard());
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            solver->push();
            const Subs &up = r.getUpdate(i);
            const VarSet &vars = util::RelevantVariables::find(r.getGuard()->vars(), {up}, r.getGuard());
            for (const Var &var: vars) {
                const auto &it = up.find(var);
                solver->add(Rel::buildEq(var, it == up.end() ? var : it->second));
            }
            Smt::Result smtRes = solver->check();
            if (smtRes == Smt::Sat) {
                std::vector<BoolExpr> newGuard;
                newGuard.emplace_back(r.getGuard());
                for (const Var &var: vars) {
                    const auto &it = up.find(var);
                    newGuard.emplace_back(buildLit(Rel::buildEq(var, (it == up.end() ? var : it->second))));
                }
                Rule nontermRule(r.getLhsLoc(), buildAnd(newGuard), Expr::NontermSymbol, sink, {});
                Proof proof;
                proof.ruleTransformationProof(r, "fixed-point processor", nontermRule, its);
                return {{nontermRule, proof}};
            }
            solver->pop();
        }
        return {};
    }

}
