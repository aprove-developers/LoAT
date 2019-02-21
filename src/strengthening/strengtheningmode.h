//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENINGMODE_H
#define LOAT_STRENGTHENINGMODE_H

#include "strengtheningtypes.h"

class StrengtheningMode {

    friend class InvarianceStrengthening;
    friend class Strengthening;

private:

    typedef StrengtheningTypes Types;
    typedef std::function<const Types::MaxSmtConstraints(const Types::SmtConstraints&, Z3Context&)> Mode;

    static const Types::MaxSmtConstraints invariance(const Types::SmtConstraints &constraints, Z3Context &context);

    static const Types::MaxSmtConstraints pseudoInvariance(const Types::SmtConstraints &constraints, Z3Context &context);

    static const Types::MaxSmtConstraints monotonicity(const Types::SmtConstraints &constraints, Z3Context &context);

    static const Types::MaxSmtConstraints pseudoMonotonicity(const Types::SmtConstraints &constraints, Z3Context &context);

};

#endif //LOAT_STRENGTHENINGMODE_H
