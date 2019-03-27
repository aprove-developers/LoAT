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
            if (Z3Toolbox::isValidImplication(r.getGuard(), r.getGuard().subs(r.getUpdate(i).toSubstitution(its)))) {
                return Rule(r.getLhsLoc(), r.getGuard(), Expression::NontermSymbol, sink, {});
            }
        }
        return {};
    }

}
