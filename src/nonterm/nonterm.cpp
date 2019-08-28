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
#include "../expr/relation.hpp"

namespace nonterm {

    option<std::pair<Rule, ForwardAcceleration::ResultKind>> NonTerm::apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        if (!Z3Toolbox::isValidImplication(r.getGuard(), {r.getCost() > 0})) {
            return {};
        }
        option<Rule> res = checkRecurrentSet(r, its, sink);
        if (res) {
            return {{res.get(), ForwardAcceleration::Success}};
        }
        option<Rule> chained;
        if (r.isLinear()) {
            chained = Chaining::chainRules(its, r, r, false).get();
            if (Z3Toolbox::checkAll(chained.get().getGuard()) != z3::check_result::sat) {
                chained = {};
            }
        }
        if (chained) {
            res = checkRecurrentSet(chained.get(), its, sink);
        }
        if (res) {
            return {{res.get(), ForwardAcceleration::SuccessWithRestriction}};
        }
        res = checkEventualRecurrentSet(r, its, sink);
        if (res) {
            return {{res.get(), ForwardAcceleration::SuccessWithRestriction}};
        }
        if (chained) {
            res = checkEventualRecurrentSet(chained.get(), its, sink);
        }
        if (res) {
            return {{res.get(), ForwardAcceleration::SuccessWithRestriction}};
        }
        res = searchFixpoint(r, its, sink);
        if (res) {
            return {{res.get(), ForwardAcceleration::SuccessWithRestriction}};
        }
        return {};
    }

    option<Rule> NonTerm::checkRecurrentSet(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            if (Z3Toolbox::isValidImplication(r.getGuard(), r.getGuard().subs(up))) {
                return {Rule(r.getLhsLoc(), r.getGuard(), Expression::NontermSymbol, sink, {})};
            }
        }
        return {};
    }

    option<Rule> NonTerm::checkEventualRecurrentSet(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        GuardList todo;
        Z3Context ctx;
        Z3Solver solver(ctx);
        for (const Expression &e: r.getGuard()) {
            solver.add(e.toZ3(ctx));
            if (Relation::isEquality(e)) {
                todo.push_back(e.lhs() - e.rhs() + 1 > 0);
                todo.push_back(e.rhs() - e.lhs() + 1 > 0);
            } else {
                todo.push_back(Relation::normalizeInequality(e));
            }
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            bool diverges = true;
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            GuardList newGuard(r.getGuard());
            for (const Expression &e: todo) {
                solver.push();
                Expression eup = e.subs(up);
                Expression ineq = e.lhs() <= eup.lhs();
                solver.push();
                solver.add(!ineq.toZ3(ctx));
                if (solver.check() == z3::check_result::unsat) {
                    // a real invariant
                    solver.pop();
                    continue;
                }
                solver.pop();
                solver.add(ineq.toZ3(ctx));
                if (solver.check() != z3::check_result::sat) {
                    diverges = false;
                    solver.pop();
                    break;
                }
                solver.add(Expression(eup.lhs() > eup.subs(up).lhs()).toZ3(ctx));
                if (solver.check() != z3::check_result::unsat) {
                    diverges = false;
                    solver.pop();
                    break;
                }
                newGuard.push_back(ineq);
                solver.pop();
            }
            if (diverges) {
                return {Rule(r.getLhsLoc(), newGuard, Expression::NontermSymbol, sink, {})};
            }
        }
        return {};
    }

    option<Rule> NonTerm::searchFixpoint(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
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
                return {Rule(r.getLhsLoc(), newGuard, Expression::NontermSymbol, sink, {})};
            }
            if (!r.isLinear()) {
                solver.pop();
            }
        }
        return {};
    }

}
