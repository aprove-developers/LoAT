#include "vareliminator.hpp"
#include "../util/ginacutils.hpp"

VarEliminator::VarEliminator(const GuardList &guard, const ExprSymbol &N, VariableManager &varMan): varMan(varMan), N(N) {
    assert(varMan.isTempVar(N));
    todoDeps.push({{}, guard});
    eliminate();
}

const ExprSymbolSet VarEliminator::findDependencies(const GuardList &guard) const {
    ExprSymbolSet res;
    res.insert(N);
    bool changed;
    do {
        changed = false;
        // compute dependencies of var
        for (const ExprSymbol &var: res) {
            option<ExprSymbol> dep;
            for (const Expression &rel: guard) {
                const Expression &ex = (rel.lhs() - rel.rhs()).expand();
                if (ex.degree(var) == 1) {
                    // we found a constraint which is linear in var, check all variables in var's coefficient
                    const Expression &coeff = ex.coeff(var, 1);
                    for (const ExprSymbol &x: coeff.getVariables()) {
                        if (varMan.isTempVar(x)) {
                            if (res.find(x) == res.end()) {
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
                        res.insert(dep.get());
                        changed = true;
                    }
                }
            }
        }
    } while (changed);
    res.erase(N);
    return res;
}

const std::set<std::pair<GiNaC::exmap, GuardList>> VarEliminator::eliminateDependency(const GiNaC::exmap &subs, const GuardList &guard) const {
    ExprSymbolSet deps = findDependencies(guard);
    for (auto it = deps.begin(); it != deps.end(); ++it) {
        BoundExtractor be(guard, *it);
        std::set<std::pair<GiNaC::exmap, GuardList>> res;
        for (const Expression &bound: be.getConstantBounds()) {
            GiNaC::exmap newSubs{{*it, bound}};
            res.insert({util::GiNaCUtils::compose(subs, newSubs), guard.subs(newSubs)});
        }
        if (!res.empty()) {
            return res;
        }
    }
    return {};
}

void VarEliminator::eliminateDependencies() {
    while (!todoDeps.empty()) {
        const std::pair<GiNaC::exmap, GuardList> current = todoDeps.top();
        const std::set<std::pair<GiNaC::exmap, GuardList>> &res = eliminateDependency(current.first, current.second);
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
        const GiNaC::exmap &subs = p.first;
        const GuardList &guard = p.second;
        BoundExtractor be(guard, N);
        if (be.getEq()) {
            res.insert(util::GiNaCUtils::compose(subs, {{N, be.getEq().get()}}));
        } else {
            for (const Expression &b: be.getUpper()) {
                res.insert(util::GiNaCUtils::compose(subs, {{N, b}}));
            }
        }
    }
}

const std::set<GiNaC::exmap> VarEliminator::getRes() const {
    return res;
}
