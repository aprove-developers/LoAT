//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENINGMODE_H
#define LOAT_STRENGTHENINGMODE_H

#include "types.h"

namespace strengthening {

    class Modes {

        friend class ConstraintBuilder;

        friend class Strengthener;

    private:

        static const Types::MaxSmtConstraints invariance(const Types::SmtConstraints &constraints, Z3Context &context);

        static const Types::MaxSmtConstraints
        pseudoInvariance(const Types::SmtConstraints &constraints, Z3Context &context);

        static const Types::MaxSmtConstraints
        monotonicity(const Types::SmtConstraints &constraints, Z3Context &context);

        static const Types::MaxSmtConstraints
        pseudoMonotonicity(const Types::SmtConstraints &constraints, Z3Context &context);

    };

}

#endif //LOAT_STRENGTHENINGMODE_H
