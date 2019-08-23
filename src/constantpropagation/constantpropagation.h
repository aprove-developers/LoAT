//
// Created by ffrohn on 3/27/19.
//

#ifndef LOAT_CONSTANT_PROPAGATION_H
#define LOAT_CONSTANT_PROPAGATION_H


#include <its/rule.h>
#include <its/itsproblem.h>

namespace constantpropagation {

    class ConstantPropagation {

    public:

        static option<std::pair<Rule, Rule>> apply(const Rule &r, const ITSProblem &its);

    private:

        ConstantPropagation(const Rule &r, const ITSProblem &its);

        option<std::pair<Rule, Rule>> apply();

        const Rule &r;

        const ITSProblem &its;

    };

}

#endif //LOAT_CONSTANT_PROPAGATION_H
