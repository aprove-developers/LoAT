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
#include "../smt/solver.hpp"
#include "../accelerate/forward.hpp"
#include "../accelerate/recurrence/dependencyorder.hpp"
#include "../analysis/chain.hpp"
#include "../util/relevantvariables.hpp"
#include "../util/result.hpp"

namespace nonterm {

    option<std::pair<Rule, Status>> NonTerm::universal(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        if (!Config::Analysis::NonTermMode && !Solver::isImplication(buildAnd(r.getGuard()), buildLit(r.getCost() > 0), its)) {
            return {};
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const ExprMap &up = r.getUpdate(i).toSubstitution(its);
            if (Solver::isImplication(buildAnd(r.getGuard()), buildAnd(r.getGuard().subs(up)), its)) {
                return {{Rule(r.getLhsLoc(), r.getGuard(), Expr::NontermSymbol, sink, {}), Success}};
            }
        }
        if (r.isLinear()) {
            Rule chained = Chaining::chainRules(its, r, r, false).get();
            const ExprMap &up = chained.getUpdate(0).toSubstitution(its);
            if (Solver::check(buildAnd(chained.getGuard()), its) == smt::Sat && Solver::isImplication(buildAnd(chained.getGuard()), buildAnd(chained.getGuard().subs(up)), its)) {
                return {{Rule(chained.getLhsLoc(), chained.getGuard(), Expr::NontermSymbol, sink, {}), PartialSuccess}};
            }
        }
        return {};
    }

    option<std::pair<Rule, Status>> NonTerm::fixedPoint(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        if (!Config::Analysis::NonTermMode && !Solver::isImplication(buildAnd(r.getGuard()), buildLit(r.getCost() > 0), its)) {
            return {};
        }
        Solver solver(its);
        for (const Rel &rel: r.getGuard()) {
            solver.add(rel);
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            solver.push();
            const ExprMap &up = r.getUpdate(i).toSubstitution(its);
            const VarSet &vars = util::RelevantVariables::find(r.getGuard(), {up}, r.getGuard(), its);
            for (const Var &var: vars) {
                const auto &it = up.find(var);
                solver.add(Rel(var, Rel::eq, it == up.end() ? var : it->second));
            }
            smt::Result smtRes = solver.check();
            if (smtRes == smt::Sat) {
                GuardList newGuard(r.getGuard());
                for (const Var &var: vars) {
                    const auto &it = up.find(var);
                    newGuard.emplace_back(var == (it == up.end() ? var : it->second));
                }
                return {{Rule(r.getLhsLoc(), newGuard, Expr::NontermSymbol, sink, {}), PartialSuccess}};
            }
            solver.pop();
        }
        return {};
    }

}
