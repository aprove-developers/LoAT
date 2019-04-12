//
// Created by ffrohn on 3/27/19.
//

#include "nonterm.hpp"
#include "../accelerate/meter/metertools.hpp"
#include "../z3/z3toolbox.hpp"
#include "../accelerate/forward.hpp"
#include "../accelerate/recurrence/dependencyorder.hpp"

namespace nonterm {

    option<std::pair<Rule, ForwardAcceleration::ResultKind>> NonTerm::apply(const Rule &r, const ITSProblem &its, const LocationIdx &sink) {
        std::map<unsigned int, unsigned int> depthMap;
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
            if (Z3Toolbox::isValidImplication(r.getGuard(), r.getGuard().subs(up))) {
                return {{Rule(r.getLhsLoc(), r.getGuard(), Expression::NontermSymbol, sink, {}), ForwardAcceleration::Success}};
            } else {
                depthMap[i] = maxDepth(r.getUpdate(i), its);
            }
        }
        for (unsigned int i = 0; i < r.getRhss().size(); i++) {
            unsigned int depth = depthMap[i];
            for (unsigned int j = 1; j <= depth; j++) {
                const GiNaC::exmap &up = r.getUpdate(i).toSubstitution(its);
                GuardList updatedGuard(r.getGuard());
                GuardList newGuard;
                for (unsigned int k = 0; k <= j; k++) {
                    newGuard.insert(newGuard.end(), updatedGuard.begin(), updatedGuard.end());
                    updatedGuard.applySubstitution(up);
                }
                if (Z3Toolbox::checkAll(newGuard) == z3::sat && Z3Toolbox::isValidImplication(newGuard, updatedGuard)) {
                    return {{Rule(r.getLhsLoc(), newGuard, Expression::NontermSymbol, sink, {}), ForwardAcceleration::SuccessWithRestriction}};
                }
            }
        }
        return {};
    }

    unsigned int NonTerm::maxDepth(const UpdateMap &up, const VariableManager &varMan) {
        if (!DependencyOrder::findOrder(varMan, up)) {
            return 1;
        }
        unsigned int max = 0;
        std::vector<VariableIdx> todo;
        std::map<VariableIdx, unsigned int> res;
        for (const auto &p: up) {
            todo.push_back(p.first);
        }
        while (!todo.empty()) {
            auto it = todo.begin();
            while (it != todo.end()) {
                bool done = true;
                unsigned int depth = 0;
                const ExprSymbol  &var = varMan.getVarSymbol(*it);
                const ExprSymbolSet &vars = up.getUpdate(*it).getVariables();
                if (vars.find(var) == vars.end()) {
                    depth = 1;
                    for (const ExprSymbol &x: vars) {
                        const VariableIdx &xi = varMan.getVarIdx(x);
                        if (xi != *it && up.find(xi) != up.end()) {
                            if (res.find(xi) == res.end()) {
                                done = false;
                                break;
                            } else {
                                depth = std::max(depth, res[xi]);
                            }
                        }
                    }
                }
                if (done) {
                    res[*it] = depth;
                    max = std::max(max, depth);
                    it = todo.erase(it);
                } else {
                    it++;
                }
            }
        }
        return max;
    }

}
