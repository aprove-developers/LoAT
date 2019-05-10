//
// Created by ffrohn on 3/27/19.
//

#ifndef LOAT_NONTERM_H
#define LOAT_NONTERM_H

#include "../its/rule.hpp"
#include "../its/itsproblem.hpp"
#include "../accelerate/forward.hpp"

namespace nonterm {

    class NonTerm {

    public:

        static option<std::pair<Rule, ForwardAcceleration::ResultKind>> apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink);

    };

}


#endif //LOAT_NONTERM_H
