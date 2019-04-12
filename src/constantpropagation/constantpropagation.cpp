//
// Created by ffrohn on 3/27/19.
//

#include "constantpropagation.hpp"
#include "../its/itsproblem.hpp"

namespace constantpropagation {

    option<std::pair<Rule, Rule>> ConstantPropagation::apply(const Rule &r, const ITSProblem &its) {
        return ConstantPropagation(r, its).apply();
    }

    ConstantPropagation::ConstantPropagation(const Rule &r, const ITSProblem &its): r(r), its(its) { }

    option<std::pair<Rule, Rule>> ConstantPropagation::apply() {
        assert(r.isSimpleLoop());
        option<Rule> current;
        Rule next = r;
        bool changed;
        unsigned int iterations = 0;
        do {
            current = next;
            changed = false;
            GiNaC::exmap update = current.get().getUpdate(0).toSubstitution(its);
            for (const auto &p: current.get().getUpdate(0)) {
                const VariableIdx &vi = p.first;
                const ExprSymbol &lhs = its.getVarSymbol(vi);
                const Expression &rhs = p.second;
                if (lhs != rhs && rhs == rhs.subs(update)) {
                    next.applySubstitution({{its.getVarSymbol(p.first), p.second}});
                    next.getUpdateMut(0)[vi] = lhs;
                    next.getGuardMut().push_back(lhs == rhs);
                    changed = true;
                }
            }
            if (changed) {
                iterations++;
            }
        } while (changed);
        if (iterations > 0) {
            GuardList initGuard;
            const GiNaC::exmap &up = r.getUpdate(0).toSubstitution(its);
            GuardList updatedGuard = r.getGuard();
            Expression updatedCost = r.getCost();
            Expression initCost = Expression(0);
            for (unsigned int i = 0; i < iterations; i++) {
                initGuard.insert(initGuard.end(), updatedGuard.begin(), updatedGuard.end());
                updatedGuard = updatedGuard.subs(up);
                initCost = initCost + updatedCost;
                updatedCost.applySubs(up);
            }
            UpdateMap initUpdate = r.getUpdate(0);
            for (unsigned int i = 1; i < iterations; i++) {
                for (auto &p: initUpdate) {
                    p.second.applySubs(up);
                }
            }
            Rule init(r.getLhsLoc(), initGuard, initCost, r.getRhsLoc(0), initUpdate);
            return {{init, next}};
        } else {
            return {};
        }
    }

}
