//
// Created by ffrohn on 3/21/19.
//

#ifndef LOAT_MERGING_RULEMERGER_H
#define LOAT_MERGING_RULEMERGER_H

#include "../its/itsproblem.hpp"

namespace merging {

    class RuleMerger {

    public:
        static bool mergeRules(ITSProblem &its);

    private:
        RuleMerger(ITSProblem &its);

        bool apply();

        std::vector<std::vector<TransIdx>> findCandidates();

        bool sameEquivalenceClass(const TransIdx &t1, const TransIdx &t2);

        bool merge(const std::vector<TransIdx> &candidates);

        option<Rule> merge(const Rule &r1, const Rule &r2);

        ITSProblem &its;

    };

}

#endif //LOAT_MERGING_RULEMERGER_H
