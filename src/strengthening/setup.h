//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_SETUP_H
#define LOAT_STRENGTHENING_SETUP_H

#include <its/rule.h>
#include <its/itsproblem.h>
#include "types.h"

namespace strengthening {

    class Setup {

    public:

        Setup(const Rule &rule, ITSProblem &its);

        const Context setup();

    private:

        const Rule &rule;
        ITSProblem &its;

        const std::vector<Rule> computePredecessors() const;

        const std::vector<GiNaC::exmap> computeUpdates() const;

        const GuardList computeConstraints() const;

        const Result splitInvariants(const GuardList &constraints, const std::vector<GiNaC::exmap> &updates) const;

        const Result splitMonotonicConstraints(
                const GuardList &invariants,
                const GuardList &nonInvariants,
                const std::vector<GiNaC::exmap> &updates) const;

        const std::vector<GuardList> buildPreconditions(const std::vector<Rule> &predecessors) const;

    };

}

#endif //LOAT_STRENGTHENING_SETUP_H
