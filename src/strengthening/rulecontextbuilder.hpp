//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_RULE_CONTEXT_BUILDER_H
#define LOAT_STRENGTHENING_RULE_CONTEXT_BUILDER_H

#include "../its/rule.hpp"
#include "../its/itsproblem.hpp"
#include "types.hpp"

namespace strengthening {

    class RuleContextBuilder {

    public:

        static const RuleContext build(const Rule &rule, ITSProblem &its);

    private:

        const Rule &rule;
        ITSProblem &its;

        RuleContextBuilder(const Rule &rule, ITSProblem &its);

        const RuleContext build() const;

        const std::vector<Rule> computePredecessors() const;

        const std::vector<GiNaC::exmap> computeUpdates() const;

        const std::vector<GuardList> buildPreconditions(const std::vector<Rule> &predecessors) const;

    };

}

#endif //LOAT_STRENGTHENING_RULE_CONTEXT_BUILDER_H
