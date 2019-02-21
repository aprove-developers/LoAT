//
// Created by ffrohn on 2/8/19.
//

#ifndef LOAT_INVARIANCE_STRENGTHENING_H
#define LOAT_INVARIANCE_STRENGTHENING_H

#include <its/rule.h>
#include <its/variablemanager.h>
#include "strengtheningtypes.h"
#include "strengtheningmode.h"

class InvarianceStrengthening {

    friend class Strengthening;

private:

    typedef StrengtheningTypes Types;

    const std::vector<Expression> &templates;
    const ExprSymbolSet &templateParams;
    const GuardList &relevantConstraints;
    const ExprSymbolSet &relevantVars;
    const std::vector<GiNaC::exmap> &updates;
    const std::vector<GuardList> &preconditions;
    const GuardList &invariants;
    const GuardList todo;
    const StrengtheningMode::Mode mode;
    VariableManager &varMan;
    std::unique_ptr<Z3Context> z3Context;

    InvarianceStrengthening(
            const std::vector<Expression> &templates,
            const ExprSymbolSet &templateParams,
            const GuardList &relevantConstraints,
            const ExprSymbolSet &relevantVars,
            const std::vector<GiNaC::exmap> &updates,
            const std::vector<GuardList> &preconditions,
            const GuardList &invariants,
            const GuardList &todo,
            const StrengtheningMode::Mode &mode,
            VariableManager &varMan
    );

    const option<Types::Invariants> apply();

    const Types::SmtConstraints buildSMTConstraints() const;

    const Types::Implication buildTemplatesInvariantImplication() const;

    const Types::Initiation constructZ3Initiation(const GuardList &premise) const;

    const std::vector<z3::expr> constructZ3Implication(const GuardList &premise, const GuardList &conclusion) const;

    const option<Types::Invariants> solve(const Types::MaxSmtConstraints &constraints) const;

    const GuardList instantiateTemplates(const z3::model &model) const;

    const Types::Invariants splitInitiallyValid(
            const std::vector<GuardList> &precondition,
            const GuardList &invariants) const;

};

#endif //LOAT_INVARIANCE_STRENGTHENING_H
