//
// Created by ffrohn on 2/18/19.
//

#ifndef LOAT_STRENGTHENING_STRENGTHENER_H
#define LOAT_STRENGTHENING_STRENGTHENER_H


#include <its/types.h>
#include <its/rule.h>
#include <its/variablemanager.h>
#include <its/itsproblem.h>
#include "types.h"
#include "constraintbuilder.h"

namespace strengthening {

    class Strengthener {

    public:

        static const std::vector<Rule> apply(const Rule &r, ITSProblem &its);

    private:

        const Context &context;

        explicit Strengthener(const Context &context);

        const std::vector<Rule> apply(const Mode &mode) const;

        const SmtConstraints buildSmtConstraints(
                const Templates &templates,
                Z3Context &context) const;

        const GuardList findRelevantConstraints(const GuardList &guard, const ExprSymbolSet &vars) const;

    };

}

#endif //LOAT_STRENGTHENING_STRENGTHENER_H
