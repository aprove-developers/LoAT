//
// Created by ffrohn on 2/18/19.
//

#ifndef LOAT_STRENGTHENINGTYPES_H
#define LOAT_STRENGTHENINGTYPES_H


#include <its/types.h>

class StrengtheningTypes {

    friend class InvarianceStrengthening;
    friend class Strengthening;

private:

    struct Implication {
        GuardList premise;
        GuardList conclusion;
    };

    struct Result {
        GuardList solved;
        GuardList failed;
    };

    struct Invariants {
        GuardList invariants;
        GuardList pseudoInvariants;
    };

    struct Template {

        Template(Expression t, ExprSymbolSet params): t(std::move(t)), params(std::move(params)) { }

        const Expression t;
        const ExprSymbolSet params;

    };

    struct SMTConstraints {
        std::vector<z3::expr> hard;
        std::vector<z3::expr> soft;
    };

};


#endif //LOAT_STRENGTHENINGTYPES_H
