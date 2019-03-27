//
// Created by ffrohn on 3/27/19.
//

#include "constantpropagation.h"
#include <its/itsproblem.h>

namespace constantpropagation {

    typedef ConstantPropagation Self;

    option<Rule> Self::apply(const Rule &r, const ITSProblem &its) {
        return ConstantPropagation(r, its).apply();
    }

    Self::ConstantPropagation(const Rule &r, const ITSProblem &its): r(r), its(its) { }

    option<Rule> Self::apply() {
        assert(r.isSimpleLoop());
        Rule current = r;
        option<Rule> res;
        bool changed;
        do {
            changed = false;
            GiNaC::exmap update = current.getUpdate(0).toSubstitution(its);
            for (const auto &p: current.getUpdate(0)) {
                const VariableIdx &vi = p.first;
                const ExprSymbol &lhs = its.getVarSymbol(vi);
                const Expression &rhs = p.second;
                if (lhs != rhs && rhs == rhs.subs(update)) {
                    if (!res) {
                        res = r;
                    }
                    res.get().applySubstitution({{its.getVarSymbol(p.first), p.second}});
                    res.get().getUpdateMut(0)[vi] = lhs;
                    res.get().getGuardMut().push_back(lhs == rhs);
                    current = res.get();
                    changed = true;
                    break;
                }
            }
        } while (changed);
        return res;
    }

}