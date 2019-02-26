//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_MODES_H
#define LOAT_STRENGTHENING_MODES_H

#include "types.h"

namespace strengthening {

    class Modes {

    public:

        static const MaxSmtConstraints invariance(const SmtConstraints &constraints, Z3Context &z3Ctx);

        static const MaxSmtConstraints pseudoInvariance(const SmtConstraints &constraints, Z3Context &z3Ctx);

        static const MaxSmtConstraints monotonicity(const SmtConstraints &constraints, Z3Context &z3Ctx);

        static const MaxSmtConstraints pseudoMonotonicity(const SmtConstraints &constraints, Z3Context &z3Ctx);

    };

}

#endif //LOAT_STRENGTHENING_MODES_H
