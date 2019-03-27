//
// Created by ffrohn on 3/27/19.
//

#ifndef LOAT_NONTERM_H
#define LOAT_NONTERM_H

#include <its/rule.h>
#include <its/itsproblem.h>

namespace nonterm {

    class NonTerm {

    public:

        static option<Rule> apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink);

    };

}


#endif //LOAT_NONTERM_H
