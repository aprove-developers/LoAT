//
// Created by ffrohn on 2/18/19.
//

#ifndef LOAT_STRENGTHENING_H
#define LOAT_STRENGTHENING_H


#include <its/types.h>
#include <its/rule.h>
#include <its/variablemanager.h>
#include <its/itsproblem.h>
#include "StrengtheningTypes.h"
#include "invarianceStrengthening.h"

class Strengthening {

public:

    static const std::vector<Rule> apply(const Rule &r, ITSProblem &its);

private:

    typedef StrengtheningTypes Types;

    const Rule &r;
    const std::vector<Rule> predecessors;
    const std::vector<GiNaC::exmap> updates;
    VariableManager &varMan;

    Strengthening(const Rule &r, ITSProblem &its);

    static const std::vector<Rule> computePredecessors(const Rule &r, ITSProblem &its);

    static const std::vector<GiNaC::exmap> computeUpdates(const Rule &r, VariableManager &varMan);

    const std::vector<Rule> apply() const;

    const Types::Result splitInvariants(const GuardList &constraints) const;

    const Types::Result splitMonotonicConstraints(const GuardList &invariants, const GuardList &nonInvariants) const;

    const std::vector<GuardList> buildPreconditions() const;

    const ExprSymbolSet findRelevantVariables(const Expression &c) const;

    const GuardList findRelevantConstraints(const GuardList &guard, const ExprSymbolSet &vars) const;

    const Types::Template buildTemplate(const ExprSymbolSet &vars) const;

    const option<Types::Invariants> tryToForceInvariance(
            const GuardList &todo,
            const std::vector<GuardList> &preconditions) const;

};

#endif //LOAT_STRENGTHENING_H
