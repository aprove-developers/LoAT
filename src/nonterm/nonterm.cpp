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
#include "../accelerate/forward.hpp"
#include "../accelerate/recurrence/dependencyorder.hpp"
#include "../analysis/chain.hpp"
#include "../util/relevantvariables.hpp"
#include "../util/result.hpp"

namespace nonterm {

    option<std::pair<Rule, Status>> NonTerm::universal(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        if (!Config::Analysis::NonTermMode && !Smt::isImplication(buildAnd(r.getGuard()), buildLit(r.getCost() > 0), its)) {
            return {};
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            if (Smt::isImplication(buildAnd(r.getGuard()), buildAnd(r.getGuard().subs(up)), its)) {
                return {{Rule(r.getLhsLoc(), r.getGuard(), Expression::NontermSymbol, sink, {}), Success}};
            }
        }
        if (r.isLinear()) {
            Rule chained = Chaining::chainRules(its, r, r, false).get();
            const GiNaC::exmap &up = chained.getUpdate(0).toSubstitution(its);
            if (Smt::check(buildAnd(chained.getGuard()), its) == Smt::Sat && Smt::isImplication(buildAnd(chained.getGuard()), buildAnd(chained.getGuard().subs(up)), its)) {
                return {{Rule(chained.getLhsLoc(), chained.getGuard(), Expression::NontermSymbol, sink, {}), PartialSuccess}};
            }
        }
        return {};
    }

    option<std::pair<Rule, Status>> NonTerm::fixedPoint(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        if (!Config::Analysis::NonTermMode && !Smt::isImplication(buildAnd(r.getGuard()), buildLit(r.getCost() > 0), its)) {
            return {};
        }
        std::unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic({r.getGuard()}, r.getUpdates()), its, Config::Z3::DefaultTimeout);
        for (const Expression &e: r.getGuard()) {
            solver->add(e);
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            solver->push();
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            const ExprSymbolSet &vars = util::RelevantVariables::find(r.getGuard(), {up}, r.getGuard(), its);
            for (const ExprSymbol &var: vars) {
                solver->add(Expression(var == var.subs(up)));
            }
            Smt::Result smtRes = solver->check();
            if (smtRes == Smt::Sat) {
                auto model = solver->model();
                for (auto p: model) {
                    std::cerr << p.first << ": " << p.second << std::endl;
                }
                GuardList newGuard(r.getGuard());
                for (const ExprSymbol &var: vars) {
                    newGuard.emplace_back(var == var.subs(up));
                }
                return {{Rule(r.getLhsLoc(), newGuard, Expression::NontermSymbol, sink, {}), PartialSuccess}};
            }
            solver->pop();
        }
        return {};
    }

}
