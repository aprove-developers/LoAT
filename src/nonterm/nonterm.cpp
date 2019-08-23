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
#include "../z3/z3toolbox.hpp"
#include "../accelerate/forward.hpp"
#include "../accelerate/recurrence/dependencyorder.hpp"
#include "../analysis/chain.hpp"
#include "../z3/z3solver.hpp"
#include "../util/relevantvariables.hpp"

namespace nonterm {

    option<std::pair<Rule, ForwardAcceleration::ResultKind>> NonTerm::apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        if (!Z3Toolbox::isValidImplication(r.getGuard(), {r.getCost() > 0})) {
            return {};
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            if (Z3Toolbox::isValidImplication(r.getGuard(), r.getGuard().subs(up))) {
                return {{Rule(r.getLhsLoc(), r.getGuard(), Expression::NontermSymbol, sink, {}), ForwardAcceleration::Success}};
            }
        }
        if (r.isLinear()) {
            Rule chained = Chaining::chainRules(its, r, r, false).get();
            const GiNaC::exmap &up = chained.getUpdate(0).toSubstitution(its);
            if (Z3Toolbox::checkAll({chained.getGuard()}) == z3::sat && Z3Toolbox::isValidImplication(chained.getGuard(), chained.getGuard().subs(up))) {
                return {{Rule(chained.getLhsLoc(), chained.getGuard(), Expression::NontermSymbol, sink, {}), ForwardAcceleration::SuccessWithRestriction}};
            }
        }
        Z3Context context;
        Z3Solver solver(context);
        for (const Expression &e: r.getGuard()) {
            solver.add(e.toZ3(context));
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            if (!r.isLinear()) {
                solver.push();
            }
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            const ExprSymbolSet &vars = util::RelevantVariables::find(r.getGuard(), {up}, r.getGuard(), its);
            for (const ExprSymbol &var: vars) {
                solver.add(Expression(var == var.subs(up)).toZ3(context));
            }
            if (solver.check() == z3::sat) {
                GuardList newGuard(r.getGuard());
                for (const ExprSymbol &var: vars) {
                    newGuard.emplace_back(var == var.subs(up));
                }
                return {{Rule(r.getLhsLoc(), newGuard, Expression::NontermSymbol, sink, {}), ForwardAcceleration::SuccessWithRestriction}};
            }
            if (!r.isLinear()) {
                solver.pop();
            }
        }
        return {};
    }

}
