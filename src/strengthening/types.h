//
// Created by ffrohn on 2/18/19.
//

#ifndef LOAT_STRENGTHENINGTYPES_H
#define LOAT_STRENGTHENINGTYPES_H


#include <its/types.h>

namespace strengthening {

    class Types {

        friend class ConstraintBuilder;

        friend class Strengthener;

        friend class Modes;

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

            Template(Expression t, ExprSymbolSet params) : t(std::move(t)), params(std::move(params)) {}

            const Expression t;
            const ExprSymbolSet params;

        };

        struct MaxSmtConstraints {
            std::vector<z3::expr> hard;
            std::vector<z3::expr> soft;
        };

        struct Initiation {
            std::vector<z3::expr> valid;
            std::vector<z3::expr> satisfiable;
        };

        struct SmtConstraints {

            SmtConstraints(
                    Initiation initiation,
                    std::vector<z3::expr> templatesInvariant,
                    std::vector<z3::expr> conclusionsInvariant,
                    std::vector<z3::expr> conclusionsMonotonic) :
                    initiation(std::move(initiation)),
                    templatesInvariant(std::move(templatesInvariant)),
                    conclusionsInvariant(std::move(conclusionsInvariant)),
                    conclusionsMonotonic(std::move(conclusionsMonotonic)) {}

            const Initiation initiation;
            const std::vector<z3::expr> templatesInvariant;
            const std::vector<z3::expr> conclusionsInvariant;
            const std::vector<z3::expr> conclusionsMonotonic;

        };

        typedef std::function<const MaxSmtConstraints(const SmtConstraints &, Z3Context &)> Mode;

    };

}

#endif //LOAT_STRENGTHENINGTYPES_H
