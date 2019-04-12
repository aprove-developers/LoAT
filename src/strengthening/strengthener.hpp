//
// Created by ffrohn on 2/18/19.
//

#ifndef LOAT_STRENGTHENING_STRENGTHENER_H
#define LOAT_STRENGTHENING_STRENGTHENER_H


#include "../its/types.hpp"
#include "../its/rule.hpp"
#include "../its/variablemanager.hpp"
#include "../its/itsproblem.hpp"
#include "types.hpp"
#include "constraintbuilder.hpp"

namespace strengthening {

    class Strengthener {

    public:

        static const std::vector<Rule> apply(const Rule &r, ITSProblem &its, const std::vector<Mode> &modes);

    private:

        const RuleContext &ruleCtx;

        explicit Strengthener(const RuleContext &ruleCtx);

        const std::vector<GuardList> apply(const Mode &mode, const GuardList &guard) const;

    };

}

#endif //LOAT_STRENGTHENING_STRENGTHENER_H
