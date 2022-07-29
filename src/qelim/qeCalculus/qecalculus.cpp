#include "qecalculus.hpp"

#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../util/relevantvariables.hpp"
#include "../../qelim/redlog.hpp"

Quantifier QeProblem::getQuantifier() const {
    return formula->getPrefix()[0];
}

RelSet QeProblem::findConsistentSubset(BoolExpr e) const {
    if (isConjunction) {
        return todo;
    }
    solver->push();
    solver->add(e);
    Smt::Result sat = solver->check();
    RelSet res;
    if (sat == Smt::Sat) {
        const Subs &model = solver->model().toSubs();
        for (const Rel &rel: todo) {
            if (rel.subs(model).isTriviallyTrue()) {
                res.insert(rel);
            }
        }
    }
    solver->pop();
    return res;
}

option<QeProblem::Entry> QeProblem::depsWellFounded(const Rel& rel, RelSet seen) const {
    if (seen.find(rel) != seen.end()) {
        return {};
    }
    seen.insert(rel);
    const auto& it = res.find(rel);
    if (it == res.end()) {
        return {};
    }
    for (const Entry& e: it->second) {
        bool success = true;
        for (const auto& dep: e.dependencies) {
            if (!depsWellFounded(dep, seen)) {
                success = false;
                break;
            }
        }
        if (success) {
            return {e};
        }
    }
    return {};
}

option<unsigned int> QeProblem::store(const Rel &rel, const RelSet &deps, const BoolExpr formula, bool exact) {
    if (res.count(rel) == 0) {
        res[rel] = std::vector<Entry>();
    }
    res[rel].push_back({deps, formula, exact});
    return res[rel].size() - 1;
}

bool QeProblem::monotonicity(const Rel &rel, const Var& n) {
    const auto bound = getQuantifier().upperBound(n);
    if (bound) {
        const Rel updated = rel.subs({n,n+1});
        const Rel newCond = rel.subs({n, bound.get()});
        RelSet premise = findConsistentSubset(boundedFormula & rel & updated & newCond);
        if (!premise.empty()) {
            BoolExprSet assumptions;
            BoolExprSet deps;
            premise.erase(rel);
            premise.erase(updated);
            for (const Rel &p: premise) {
                const BoolExpr lit = buildLit(p);
                assumptions.insert(lit);
                deps.insert(lit);
            }
            assumptions.insert(buildLit(updated));
            assumptions.insert(buildLit(!rel));
            const BoolExprSet &unsatCore = Smt::unsatCore(assumptions, varMan);
            if (!unsatCore.empty()) {
                RelSet dependencies;
                for (const BoolExpr &e: unsatCore) {
                    if (deps.count(e) > 0) {
                        const RelSet &lit = e->lits();
                        assert(lit.size() == 1);
                        dependencies.insert(*lit.begin());
                    }
                }
                const BoolExpr newGuard = buildLit(newCond);
                option<unsigned int> idx = store(rel, dependencies, newGuard);
                if (idx) {
                    std::stringstream ss;
                    ss << rel << " [" << idx.get() << "]: montonic decrease yields " << newGuard;
                    if (!dependencies.empty()) {
                        ss << ", dependencies:";
                        for (const Rel &rel: dependencies) {
                            ss << " " << rel;
                        }
                    }
                    proof.newline();
                    proof.append(ss);
                    return true;
                }
            }
        }
    }
    return false;
}

bool QeProblem::recurrence(const Rel &rel, const Var& n) {
    const auto bound = getQuantifier().lowerBound(n);
    if (bound) {
        const Rel updated = rel.subs({n, n+1});
        const Rel newCond = rel.subs({n, bound.get()});
        RelSet premise = findConsistentSubset(boundedFormula & rel & updated & newCond);
        if (!premise.empty()) {
            BoolExprSet deps;
            BoolExprSet assumptions;
            premise.erase(rel);
            premise.erase(updated);
            for (const Rel &p: premise) {
                const BoolExpr b = buildLit(p);
                assumptions.insert(b);
                deps.insert(b);
            }
            assumptions.insert(buildLit(rel));
            assumptions.insert(buildLit(!updated));
            BoolExprSet unsatCore = Smt::unsatCore(assumptions, varMan);
            if (!unsatCore.empty()) {
                RelSet dependencies;
                for (const BoolExpr &e: unsatCore) {
                    if (deps.count(e) > 0) {
                        const RelSet &lit = e->lits();
                        assert(lit.size() == 1);
                        dependencies.insert(*lit.begin());
                    }
                }
                dependencies.erase(rel);
                const BoolExpr newGuard = buildLit(newCond);
                option<unsigned int> idx = store(rel, dependencies, newGuard);
                if (idx) {
                    std::stringstream ss;
                    ss << rel << " [" << idx.get() << "]: monotonic increase yields " << newGuard;
                    if (!dependencies.empty()) {
                        ss << ", dependencies:";
                        for (const Rel &rel: dependencies) {
                            ss << " " << rel;
                        }
                    }
                    proof.newline();
                    proof.append(ss);
                    return true;
                }
            }
        }
    }
    return false;
}

bool QeProblem::eventualWeakDecrease(const Rel &rel, const Var& n) {
    if (depsWellFounded(rel)) {
        return false;
    }
    const auto lowerBound = getQuantifier().lowerBound(n);
    const auto upperBound = getQuantifier().upperBound(n);
    if (lowerBound && upperBound) {
        const Expr updated = rel.lhs().subs({n, n+1});
        const Rel dec = rel.lhs() >= updated;
        const Rel inc = updated < updated.subs({n, n+1});
        const BoolExpr newGuard = buildLit(rel.subs({n, lowerBound.get()})) & rel.subs({n, upperBound.get()});
        RelSet premise = findConsistentSubset(boundedFormula & dec & !inc & newGuard);
        if (!premise.empty()) {
            BoolExprSet assumptions;
            BoolExprSet deps;
            premise.erase(rel);
            premise.erase(dec);
            premise.erase(!inc);
            for (const Rel &p: premise) {
                const BoolExpr lit = buildLit(p);
                assumptions.insert(lit);
                deps.insert(lit);
            }
            assumptions.insert(buildLit(dec));
            assumptions.insert(buildLit(inc));
            BoolExprSet unsatCore = Smt::unsatCore(assumptions, varMan);
            if (!unsatCore.empty()) {
                RelSet dependencies;
                for (const BoolExpr &e: unsatCore) {
                    if (deps.count(e) > 0) {
                        const RelSet &lit = e->lits();
                        assert(lit.size() == 1);
                        dependencies.insert(*lit.begin());
                    }
                }
                option<unsigned int> idx = store(rel, dependencies, newGuard);
                if (idx) {
                    std::stringstream ss;
                    ss << rel << " [" << idx.get() << "]: eventual decrease yields " << newGuard;
                    if (!dependencies.empty()) {
                        ss << ", dependencies:";
                        for (const Rel &rel: dependencies) {
                            ss << " " << rel;
                        }
                    }
                    proof.newline();
                    proof.append(ss);
                    return true;
                }
            }
        }
    }
    return false;
}

bool QeProblem::eventualWeakIncrease(const Rel &rel, const Var& n) {
    if (depsWellFounded(rel)) {
        return false;
    }
    const auto bound = getQuantifier().lowerBound(n);
    if (bound) {
        const Expr updated = rel.lhs().subs({n, n+1});
        const Rel inc = rel.lhs() <= updated;
        const Rel dec = updated > updated.subs({n, n+1});
        const Rel newCond = rel.subs({n, bound.get()});
        RelSet premise = findConsistentSubset(boundedFormula & inc & !dec & newCond);
        if (!premise.empty()) {
            BoolExprSet assumptions;
            BoolExprSet deps;
            premise.erase(rel);
            premise.erase(inc);
            premise.erase(!dec);
            for (const Rel &p: premise) {
                const BoolExpr lit = buildLit(p);
                assumptions.insert(lit);
                deps.insert(lit);
            }
            assumptions.insert(buildLit(dec));
            assumptions.insert(buildLit(inc));
            BoolExprSet unsatCore = Smt::unsatCore(assumptions, varMan);
            if (!unsatCore.empty()) {
                RelSet dependencies;
                for (const BoolExpr &e: unsatCore) {
                    if (deps.count(e) > 0) {
                        const RelSet &lit = e->lits();
                        assert(lit.size() == 1);
                        dependencies.insert(*lit.begin());
                    }
                }
                const BoolExpr newGuard = buildLit(newCond) & inc.subs({n, bound.get()});
                if (Smt::check(newGuard, varMan) == Smt::Sat) {
                    option<unsigned int> idx = store(rel, dependencies, newGuard, false);
                    if (idx) {
                        std::stringstream ss;
                        ss << rel << " [" << idx.get() << "]: eventual increase yields " << newGuard;
                        if (!dependencies.empty()) {
                            ss << ", dependencies:";
                            for (const Rel &rel: dependencies) {
                                ss << " " << rel;
                            }
                        }
                        proof.newline();
                        proof.append(ss);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool QeProblem::fixpoint(const Rel &rel, const Var& n) {
    if (res.find(rel) == res.end() && rel.isPoly()) {
        const auto lhs = rel.lhs().expand();
        unsigned degree = lhs.degree(n);
        BoolExpr vanish = True;
        Subs subs;
        for (unsigned d = 1; d <= degree; ++d) {
            vanish = vanish & (Rel::buildEq(lhs.coeff(n, d), 0));
        }
        const auto constant = lhs.subs({n, 0}) > 0;
        if (Smt::check(boundedFormula & constant & vanish, varMan) == Smt::Sat) {
            BoolExpr newGuard = buildLit(constant) & vanish;
            option<unsigned int> idx = store(rel, {}, newGuard, false);
            if (idx) {
                std::stringstream ss;
                ss << rel << " [" << idx.get() << "]: fixpoint yields " << newGuard;
                proof.newline();
                proof.append(ss);
                return true;
            }
        }
    }
    return false;
}

QeProblem::ReplacementMap QeProblem::computeReplacementMap() const {
    ReplacementMap res;
    res.exact = boundedFormula->isConjunction();
    RelMap<Entry> entryMap;
    for (const Rel& rel: todo) {
        option<Entry> e = depsWellFounded(rel);
        if (e) {
            entryMap[rel] = e.get();
            res.exact &= e->exact;
        } else {
            res.map[rel] = False;
            res.exact = false;
            if (isConjunction) return res;
        }
    }
    if (!isConjunction) {
        bool changed;
        do {
            changed = false;
            for (auto e: entryMap) {
                if (res.map.find(e.first) != res.map.end()) continue;
                BoolExpr closure = e.second.formula;
                bool ready = true;
                for (auto dep: e.second.dependencies) {
                    if (res.map.find(dep) == res.map.end()) {
                        ready = false;
                        break;
                    } else {
                        closure = closure & res.map[dep];
                    }
                }
                if (ready) {
                    res.map[e.first] = closure;
                    changed = true;
                }
            }
        } while (changed);
    } else {
        for (auto e: entryMap) {
            res.map[e.first] = e.second.formula;
        }
    }
    return res;
}

option<Qelim::Result> QeProblem::qe(const QuantifiedFormula &qf) {
    formula = qf;
    const auto quantifiers = formula->getPrefix();
    if (quantifiers.size() > 1) {
        return {};
    }
    const auto quantifier = getQuantifier();
    if (quantifier.getType() != Quantifier::Type::Forall) {
        return {};
    }
    const auto vars = quantifier.getVars();
    auto matrix = formula->getMatrix();
    isConjunction = matrix->isConjunction();
    bool exact = true;
    for (const auto& var: vars) {
        res = {};
        solution = {};
        todo = matrix->lits();
        boundedFormula = matrix;
        const auto lowerBound = quantifier.lowerBound(var);
        if (lowerBound) {
            boundedFormula = boundedFormula & (lowerBound.get() <= var);
        }
        const auto upperBound = quantifier.upperBound(var);
        if (upperBound) {
            boundedFormula = boundedFormula & (var <= upperBound.get());
        }
        for (const Rel& rel: todo) {
            bool res = recurrence(rel, var);
            res |= monotonicity(rel, var);
            res |= eventualWeakDecrease(rel, var);
            res |= eventualWeakIncrease(rel, var);
            res |= fixpoint(rel, var);
            if (!res && isConjunction) return {};
        }
        ReplacementMap map = computeReplacementMap();
        matrix = matrix->replaceRels(map.map);
        if (Smt::check(matrix, varMan) != Smt::Sat) {
            return {};
        }
        exact &= map.exact;
    }
    return Qelim::Result(matrix, proof, exact);
}

Proof QeProblem::getProof() const {
    return proof;
}

