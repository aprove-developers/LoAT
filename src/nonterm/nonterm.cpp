//
// Created by ffrohn on 3/27/19.
//

#include "nonterm.hpp"
#include "../accelerate/meter/metertools.hpp"
#include "../z3/z3toolbox.hpp"
#include "../accelerate/forward.hpp"
#include "../accelerate/recurrence/dependencyorder.hpp"
#include "../analysis/chain.hpp"

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
        return {};
    }

}
