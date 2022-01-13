#include "accelerationproblem.hpp"
#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../util/relevantvariables.hpp"

AccelerationProblem::AccelerationProblem(
        const BoolExpr guard,
        const Subs &up,
        option<const Subs&> closed,
        const Expr &cost,
        const Expr &iteratedCost,
        const Var &n,
        const unsigned int validityBound,
        ITSProblem &its): todo(guard->lits()), up(up), closed(closed), cost(cost), iteratedCost(iteratedCost), n(n), guard(guard), validityBound(validityBound), its(its) {
    std::vector<Subs> subs = closed.map([&up](auto const &closed){return std::vector<Subs>{up, closed};}).get_value_or({up});
    Smt::Logic logic = Smt::chooseLogic<RelSet, Subs>({todo}, subs);
    this->solver = SmtFactory::modelBuildingSolver(logic, its);
    this->solver->add(guard);
    this->isConjunction = guard->isConjunction();
    this->proof.append(std::stringstream() << "accelerating " << guard << " wrt. " << up);
}

option<AccelerationProblem> AccelerationProblem::init(const LinearRule &r, ITSProblem &its) {
    const Var &n = its.addFreshTemporaryVariable("n");
    const option<Recurrence::Result> &res = Recurrence::iterateRule(its, r, n);
    if (res) {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        option<const Subs&>(res->update),
                        r.getCost(),
                        res->cost,
                        n,
                        res->validityBound,
                        its)};
    } else {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        option<const Subs&>(),
                        r.getCost(),
                        r.getCost(),
                        n,
                        0,
                        its)};
    }
}

AccelerationProblem AccelerationProblem::initForRecurrentSet(const LinearRule &r, ITSProblem &its) {
    return AccelerationProblem(
                r.getGuard()->toG(),
                r.getUpdate(),
                option<const Subs&>(),
                r.getCost(),
                r.getCost(),
                its.addFreshTemporaryVariable("n"),
                0,
                its);
}

RelSet AccelerationProblem::findConsistentSubset(BoolExpr e) const {
    if (isConjunction) {
        return todo;
    }
    solver->push();
    solver->add(e);
    Smt::Result sat = solver->check();
    solver->pop();
    if (sat == Smt::Sat) {
        RelSet res;
        const Subs &model = solver->model().toSubs();
        for (const Rel &rel: todo) {
            if (rel.subs(model).isTriviallyTrue()) {
                res.insert(rel);
            }
        }
        return res;
    }
    return RelSet();
}

bool AccelerationProblem::Entry::subsumes(const Entry &that) const {
    if (that.nonterm && !this->nonterm) {
        return false;
    }
    for (const Rel &rel: this->dependencies) {
        if (that.dependencies.count(rel) == 0) {
            return false;
        }
    }
    return true;
}

option<unsigned int> AccelerationProblem::store(const Rel &rel, const RelSet &deps, const BoolExpr formula, bool nonterm) {
    if (res.count(rel) == 0) {
        res[rel] = std::vector<Entry>();
    }
    Entry newE{deps, formula, nonterm, true};
    for (Entry &e: res[rel]) {
        if (e.active) {
            if (e.subsumes(newE)) {
                return {};
            }
            if (newE.subsumes(e)) {
                e.active = false;
            }
        }
    }
    res[rel].emplace_back(newE);
    return res[rel].size() - 1;
}

option<AccelerationProblem::Entry> AccelerationProblem::depsWellFounded(const Rel& rel, bool nontermOnly, RelSet seen) {
    if (seen.find(rel) != seen.end()) {
        return {};
    }
    seen.insert(rel);
    const auto& it = res.find(rel);
    if (it == res.end()) {
        return {};
    }
    for (const Entry& e: it->second) {
        if (!e.active || (nontermOnly && !e.nonterm)) continue;
        bool success = true;
        for (const auto& dep: e.dependencies) {
            if (!depsWellFounded(dep, nontermOnly, seen)) {
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

bool AccelerationProblem::monotonicity(const Rel &rel) {
    if (closed) {
        if (depsWellFounded(rel)) {
            return false;
        }
        const Rel updated = rel.subs(up);
        const Rel newCond = rel.subs(closed.get()).subs(Subs(n, n-1));
        RelSet premise = findConsistentSubset(guard & rel & updated & newCond & (n >= validityBound));
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
            const BoolExprSet &unsatCore = Smt::unsatCore(assumptions, its);
            if (!unsatCore.empty()) {
                RelSet dependencies;
                for (const BoolExpr &e: unsatCore) {
                    if (deps.count(e) > 0) {
                        const RelSet &lit = e->lits();
                        assert(lit.size() == 1);
                        dependencies.insert(*lit.begin());
                    }
                }
                const BoolExpr newGuard = buildAnd(dependencies) & newCond;
                if (Smt::check(newGuard & (n >= validityBound), its) == Smt::Sat) {
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
    }
    return false;
}

bool AccelerationProblem::recurrence(const Rel &rel) {
    const Rel updated = rel.subs(up);
    RelSet premise = findConsistentSubset(guard & rel & updated);
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
        BoolExprSet unsatCore = Smt::unsatCore(assumptions, its);
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
            const BoolExpr newGuard = buildAnd(dependencies) & rel;
            option<unsigned int> idx = store(rel, dependencies, newGuard, true);
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
    return false;
}

bool AccelerationProblem::eventualWeakDecrease(const Rel &rel) {
    if (closed) {
        if (depsWellFounded(rel)) {
             return false;
        }
        const Expr updated = rel.lhs().subs(up);
        const Rel dec = rel.lhs() >= updated;
        const Rel inc = updated < updated.subs(up);
        const Rel newCond = rel.subs(closed.get()).subs(Subs(n, n-1));
        RelSet premise = findConsistentSubset(guard & dec & !inc & rel & newCond & (n >= validityBound));
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
            BoolExprSet unsatCore = Smt::unsatCore(assumptions, its);
            if (!unsatCore.empty()) {
                RelSet dependencies;
                for (const BoolExpr &e: unsatCore) {
                    if (deps.count(e) > 0) {
                        const RelSet &lit = e->lits();
                        assert(lit.size() == 1);
                        dependencies.insert(*lit.begin());
                    }
                }
                const BoolExpr newGuard = buildAnd(dependencies) & rel & newCond;
                if (Smt::check(newGuard & (n >= validityBound), its) == Smt::Sat) {
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
    }
    return false;
}

bool AccelerationProblem::eventualWeakIncrease(const Rel &rel) {
    if (depsWellFounded(rel, true)) {
        return false;
    }
    const Expr &updated = rel.lhs().subs(up);
    const Rel &inc = rel.lhs() <= updated;
    const Rel &dec = updated > updated.subs(up);
    RelSet premise = findConsistentSubset(guard & inc & !dec & rel);
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
        BoolExprSet unsatCore = Smt::unsatCore(assumptions, its);
        if (!unsatCore.empty()) {
            RelSet dependencies;
            for (const BoolExpr &e: unsatCore) {
                if (deps.count(e) > 0) {
                    const RelSet &lit = e->lits();
                    assert(lit.size() == 1);
                    dependencies.insert(*lit.begin());
                }
            }
            const BoolExpr newGuard = buildAnd(dependencies) & rel & inc;
            if (Smt::check(newGuard, its) == Smt::Sat) {
                option<unsigned int> idx = store(rel, dependencies, newGuard, true);
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
    return false;
}

bool AccelerationProblem::fixpoint(const Rel &rel) {
    if (res.find(rel) == res.end()) {
        RelSet eqs;
        VarSet vars = util::RelevantVariables::find(rel.vars(), {up}, True);
        for (const Var& var: vars) {
            eqs.insert(Rel::buildEq(var, Expr(var).subs(up)));
        }
        BoolExpr allEq = buildAnd(eqs);
        if (Smt::check(guard & rel & allEq, its) == Smt::Sat) {
            BoolExpr newGuard = allEq & rel;
            option<unsigned int> idx = store(rel, {}, newGuard, true);
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

std::vector<AccelerationProblem::Result> AccelerationProblem::computeRes() {
    for (const Rel& rel: todo) {
        bool res = recurrence(rel);
        res |= monotonicity(rel);
        res |= eventualWeakDecrease(rel);
        res |= eventualWeakIncrease(rel);
        res |= fixpoint(rel);
        if (!res && isConjunction) return {};
    }
    std::vector<AccelerationProblem::Result> ret;
    bool nonterm = true;
    RelMap<BoolExpr> map;
    bool all = true;
    for (const Rel& rel: todo) {
        option<Entry> e = depsWellFounded(rel);
        if (e) {
            map[rel] = e.get().formula;
            nonterm &= e->nonterm;
        } else {
            all = false;
            if (isConjunction) break;
            map[rel] = False;
        }
    }
    if (all || !isConjunction) {
        bool positiveCost = Config::Analysis::mode != Config::Analysis::Mode::Complexity || Smt::isImplication(guard, buildLit(cost > 0), its);
        BoolExpr newGuard = guard->replaceRels(map) & (n >= validityBound);
        if (Smt::check(newGuard, its) == Smt::Sat) {
            ret.emplace_back(newGuard, nonterm && positiveCost);
        }
        if (closed && positiveCost && !nonterm) {
            RelMap<BoolExpr> map;
            all = true;
            for (const Rel& rel: todo) {
                option<Entry> e = depsWellFounded(rel, true);
                if (e) {
                    map[rel] = e.get().formula;
                } else {
                    all = false;
                    if (isConjunction) break;
                    map[rel] = False;
                }
            }
            if (all || !isConjunction) {
                BoolExpr newGuard = guard->replaceRels(map);
                if (Smt::check(newGuard, its) == Smt::Sat) {
                    ret.emplace_back(newGuard, true);
                }
            }
        }
    }
    return ret;
}

Proof AccelerationProblem::getProof() const {
    return proof;
}

Expr AccelerationProblem::getAcceleratedCost() const {
    return iteratedCost;
}

option<Subs> AccelerationProblem::getClosedForm() const {
    return closed;
}

Var AccelerationProblem::getIterationCounter() const {
    return n;
}

unsigned int AccelerationProblem::getValidityBound() const {
    return validityBound;
}
