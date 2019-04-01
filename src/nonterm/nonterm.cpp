//
// Created by ffrohn on 3/27/19.
//

#include <accelerate/meter/metertools.h>
#include <z3/z3toolbox.h>
#include <accelerate/forward.h>
#include "nonterm.h"

namespace nonterm {

    typedef NonTerm Self;

    // TODO do not use a fixed number of unrolings
    option<std::pair<Rule, ForwardAcceleration::ResultKind>> Self::apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            if (Z3Toolbox::isValidImplication(r.getGuard(), r.getGuard().subs(up))) {
                return {{Rule(r.getLhsLoc(), r.getGuard(), Expression::NontermSymbol, sink, {}), ForwardAcceleration::Success}};
            }
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            const GuardList &updatedGuard = r.getGuard().subs(up);
            if (Z3Toolbox::checkAll(updatedGuard) == z3::sat && Z3Toolbox::isValidImplication(updatedGuard, updatedGuard.subs(up))) {
                GuardList newGuard = r.getGuard();
                newGuard.insert(newGuard.end(), updatedGuard.begin(), updatedGuard.end());
                return {{Rule(r.getLhsLoc(), newGuard, Expression::NontermSymbol, sink, {}), ForwardAcceleration::SuccessWithRestriction}};
            }
        }
        return {};
    }

}
