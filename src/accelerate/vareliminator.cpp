#include "vareliminator.hpp"

VarEliminator::VarEliminator(const GuardList &guard, const ExprSymbol &N, VariableManager &varMan): varMan(varMan), N(N) {
    assert(varMan.isTempVar(N));
    todoDeps.push({{}, guard});
    findDependencies(guard);
    eliminate();
}

void VarEliminator::findDependencies(const GuardList &guard) {
    dependencies.insert(N);
    bool changed;
    do {
        changed = false;
        // compute dependencies of var
        for (const ExprSymbol &var: dependencies) {
            option<ExprSymbol> dep;
            for (const Rel &rel: guard) {
                const Expression &ex = (rel.lhs() - rel.rhs()).expand();
                if (ex.degree(var) == 1) {
                    // we found a constraint which is linear in var, check all variables in var's coefficient
                    const Expression &coeff = ex.coeff(var, 1);
                    for (const ExprSymbol &x: coeff.getVariables()) {
                        if (varMan.isTempVar(x)) {
                            if (dependencies.find(x) == dependencies.end()) {
                                // we found a tmp variable in coeff which has not yet been marked as dependency
                                dep = x;
                            }
                        } else {
                            // coeff also contains non-tmp variables, ignore the current constraint
                            dep = {};
                            break;
                        }
                    }
                    if (dep) {
                        dependencies.insert(dep.get());
                        changed = true;
                    }
                }
            }
        }
    } while (changed);
    dependencies.erase(N);
}

const std::set<std::pair<ExprMap, GuardList>> VarEliminator::eliminateDependency(const ExprMap &subs, const GuardList &guard) const {
    ExprSymbolSet vars;
    guard.collectVariables(vars);
    for (auto it = dependencies.begin(); it != dependencies.end(); ++it) {
        if (vars.find(*it) == vars.end()) {
            continue;
        }
        BoundExtractor be(guard, *it);
        std::set<std::pair<ExprMap, GuardList>> res;
        for (const Expression &bound: be.getConstantBounds()) {
            ExprMap newSubs(*it, bound);
            res.insert({subs.compose(newSubs), guard.subs(newSubs)});
        }
        if (!res.empty()) {
            return res;
        }
    }
    return {};
}

void VarEliminator::eliminateDependencies() {
    while (!todoDeps.empty()) {
        const std::pair<ExprMap, GuardList> current = todoDeps.top();
        const std::set<std::pair<ExprMap, GuardList>> &res = eliminateDependency(current.first, current.second);
        if (res.empty()) {
            todoN.insert(current);
        }
        todoDeps.pop();
        for (const auto &p: res) {
            todoDeps.push(p);
        }
    }
}

void VarEliminator::eliminate() {
    eliminateDependencies();
    for (const auto &p: todoN) {
        const ExprMap &subs = p.first;
        const GuardList &guard = p.second;
        BoundExtractor be(guard, N);
        if (be.getEq()) {
            res.insert(subs.compose(ExprMap(N, be.getEq().get())));
        } else {
            for (const Expression &b: be.getUpper()) {
                res.insert(subs.compose(ExprMap(N, b)));
            }
        }
    }
}

const std::set<ExprMap> VarEliminator::getRes() const {
    return res;
}
