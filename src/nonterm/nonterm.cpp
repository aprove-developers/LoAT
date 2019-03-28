//
// Created by ffrohn on 3/27/19.
//

#include <accelerate/meter/metertools.h>
#include <z3/z3toolbox.h>
#include "nonterm.h"

namespace nonterm {

    typedef NonTerm Self;

    option<Rule> Self::apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            const GuardList &updatedGuard = r.getGuard().subs(up);
            if (Z3Toolbox::isValidImplication(updatedGuard, updatedGuard.subs(up))) {
                GuardList newGuard = r.getGuard();
                newGuard.insert(newGuard.end(), updatedGuard.begin(), updatedGuard.end());
                return Rule(r.getLhsLoc(), newGuard, Expression::NontermSymbol, sink, {});
            }
        }
        return {};
    }

}
