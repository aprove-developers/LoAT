//
// Created by ffrohn on 3/27/19.
//

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
                const z3::model &model = solver.get_model();
                for (const ExprSymbol &var: vars) {
                    const z3::expr &z3var = context.getVariable(var).get();
                    const Expression &pi = Z3Toolbox::getRealFromModel(model, z3var);
                    newGuard.emplace_back(var == pi);
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
