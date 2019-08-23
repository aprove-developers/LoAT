//
// Created by ffrohn on 3/27/19.
//

#ifndef LOAT_NONTERM_H
#define LOAT_NONTERM_H

#include <its/rule.h>
#include <its/itsproblem.h>
#include <accelerate/forward.h>

namespace nonterm {

    class NonTerm {

    public:

        static option<std::pair<Rule, ForwardAcceleration::ResultKind>> apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink);

    private:

        static unsigned int maxDepth(const UpdateMap &up, const VariableManager &varMan);

    };

}


#endif //LOAT_NONTERM_H
