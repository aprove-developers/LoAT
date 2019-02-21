//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_MODE_H
#define LOAT_STRENGTHENING_MODE_H

#include "types.h"

namespace strengthening {

    class Modes {

    public:

        static const MaxSmtConstraints invariance(const SmtConstraints &constraints, Z3Context &context);

        static const MaxSmtConstraints
        pseudoInvariance(const SmtConstraints &constraints, Z3Context &context);

        static const MaxSmtConstraints
        monotonicity(const SmtConstraints &constraints, Z3Context &context);

        static const MaxSmtConstraints
        pseudoMonotonicity(const SmtConstraints &constraints, Z3Context &context);

    };

}

#endif //LOAT_STRENGTHENING_MODE_H
