//
// Created by ffrohn on 3/14/19.
//

#ifndef LOAT_UTIL_RELEVANT_VARIABLES_H
#define LOAT_UTIL_RELEVANT_VARIABLES_H

#include <expr/expression.h>
#include <its/types.h>
#include <its/rule.h>

namespace util {

    class RelevantVariables {

    public:

        static const ExprSymbolSet find(
                const GuardList &constraints,
                const std::vector<GiNaC::exmap> &updates,
                const GuardList &guard,
                const VariableManager &varMan);

        static const ExprSymbolSet find(
                const GuardList &constraints,
                const std::vector<UpdateMap> &updates,
                const GuardList &guard,
                const VariableManager &varMan);

        static const ExprSymbolSet find(
                const GuardList &constraints,
                const std::vector<RuleRhs> &rhss,
                const GuardList &guard,
                const VariableManager &varMan);

    };

}

#endif //LOAT_UTIL_RELEVANT_VARIABLES_H
