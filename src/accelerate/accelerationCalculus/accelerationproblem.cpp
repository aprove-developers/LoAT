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
    solver->check();
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
    solver->push();
    solver->add(e);
    Smt::Result sat = solver->check();
    solver->pop();
    if (sat == Smt::Sat) {
        if (guard->isConjunction()) {
            return todo;
        } else {
            RelSet res;
            const Subs &model = solver->model().toSubs();
            for (const Rel &rel: todo) {
                if (rel.subs(model).isTriviallyTrue()) {
                    res.insert(rel);
                }
            }
            return res;
        }
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
    res[rel].push_back(newE);
    return res[rel].size() - 1;
}

bool AccelerationProblem::monotonicity(const Rel &rel) {
    if (closed) {
        const auto it = res.find(rel);
        auto no_deps = [](auto const &e){return e.dependencies.empty();};
        if (it != res.end() && std::any_of(it->second.begin(), it->second.end(), no_deps)) {
            return false;
        }
        const Rel updated = rel.subs(up);
        const Rel newCond = rel.subs(closed.get()).subs(Subs(n, n-1));
        RelSet premise = findConsistentSubset(guard & rel & updated & newCond & n >= validityBound);
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
        const auto it = res.find(rel);
        auto no_deps = [](auto const &e){return e.dependencies.empty();};
        if (it != res.end() && std::any_of(it->second.begin(), it->second.end(), no_deps)) {
            return false;
        }
        const Expr updated = rel.lhs().subs(up);
        const Rel dec = rel.lhs() >= updated;
        const Rel inc = updated < updated.subs(up);
        const Rel newCond = rel.subs(closed.get()).subs(Subs(n, n-1));
        RelSet premise = findConsistentSubset(guard & dec & !inc & rel & newCond & n >= validityBound);
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

bool AccelerationProblem::eventualWeakIncrease(const Rel &rel) {
    const auto it = res.find(rel);
    auto no_deps = [](auto const &e){return e.nonterm && e.dependencies.empty();};
    if (it != res.end() && std::any_of(it->second.begin(), it->second.end(), no_deps)) {
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
        solver->push();
        solver->add(rel);
        solver->add(allEq);
        Smt::Result sat = solver->check();
        solver->pop();
        if (sat == Smt::Sat) {
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

bool AccelerationProblem::checkCycle(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars) {
    RelSet done;
    const Model &m = solver->model();
    for (const Rel &rel: todo) {
        RelSet reachable;
        for (const Rel &rel2: todo) {
            if (rel == rel2) continue;
            const unsigned int x = edgeVars.at({rel, rel2})->getConst().get();
            if (m.contains(x) && m.get(x)) {
                reachable.insert(rel2);
            }
        }
        bool changed;
        do {
            changed = false;
            for (const Rel &rel1: reachable) {
                for (const Rel &rel2: todo) {
                    const unsigned int x = edgeVars.at({rel1, rel2})->getConst().get();
                    if (m.contains(x) && m.get(x)) {
                        if (rel == rel2) {
                            return true;
                        } else {
                            const auto &res = reachable.insert(rel2);
                            if (res.second) {
                                changed = true;
                            }
                        }
                    }
                }
            }
        } while (changed);
    }
    return false;
}

using Vars = std::vector<BoolExpr>;
using Edge = std::pair<Rel, Rel>;

void AccelerationProblem::encodeAcyclicity(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars) {
    // if a->b and b->c is enabled, then a->c needs to be enabled, too
    for (const auto &p: edgeVars) {
        const Edge &edge = p.first;
        const Rel &start = edge.first;
        const Rel &join = edge.second;
        const BoolExpr var1 = p.second;
        for (const Rel &target: todo) {
            if (target == join) continue;
            const BoolExpr var2 = edgeVars.at({join, target});
            const BoolExpr var3 = edgeVars.at({start, target});
            solver->add((!var1) | (!var2) | var3);
        }
    }
}

Model AccelerationProblem::enlargeSolution(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars, const BoolExprSet &soft) {
    Model model = solver->model();
    if (guard->isConjunction()) {
        return model;
    }
    for (const BoolExpr &s: soft) {
        solver->push();
        solver->add(s);
        Smt::Result satRes = solver->check();
        if (satRes == Smt::Sat && !checkCycle(edgeVars)) {
            model = solver->model();
        } else {
            solver->pop();
        }
    }
    return model;
}

Smt::Result AccelerationProblem::checkSat(const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars, const RelMap<BoolExpr> &boolAbstractionMap) {
    BoolExpr boolAbstraction = guard->replaceRels(boolAbstractionMap);
    solver->add(boolAbstraction);
    Smt::Result satRes = solver->check();
    std::vector<AccelerationProblem::Result> ret;
    if (satRes == Smt::Sat && checkCycle(edgeVars)) {
        encodeAcyclicity(edgeVars);
        satRes = solver->check();
    }
    return satRes;
}

option<AccelerationProblem::Result> AccelerationProblem::enlargeSolutionAndGetRes(const std::map<Rel, Vars> &entryVars, const std::map<std::pair<Rel, Rel>, BoolExpr> &edgeVars, const BoolExprSet &soft) {
    Model model = enlargeSolution(edgeVars, soft);
    const auto p = buildRes(model, entryVars);
    const BoolExpr& newGuard = p.first;
    bool nonterm = p.second;
    // TODO it would be better to encode satisfiability of the resulting guard in the constraint system
    if (Smt::check(newGuard, its) == Smt::Sat) {
        return {{newGuard, nonterm}};
    } else {
        return {};
    }
}

std::vector<AccelerationProblem::Result> AccelerationProblem::computeRes() {
    for (const Rel& rel: todo) {
        bool res = recurrence(rel);
        res |= monotonicity(rel);
        res |= eventualWeakDecrease(rel);
        res |= eventualWeakIncrease(rel);
        res |= fixpoint(rel);
        if (!res && guard->isConjunction()) return {};
    }
    std::map<Edge, BoolExpr> edgeVars;
    std::map<Rel, Vars> entryVars;
    for (const Rel &rel1: todo) {
        for (const Rel &rel2: todo) {
            edgeVars[{rel1, rel2}] = its.freshBoolVar();
        }
    }
    // if an entry is enabled, then the edges corresponding to its dependencies have to be enabled.
    // maps every constraint to its 'boolean abstraction'
    // which states that one of the entries corresponding to the constraint needs to be enabled
    RelMap<BoolExpr> boolAbstractionMap;
    RelMap<BoolExpr> boolNontermAbstractionMap;
    BoolExprSet soft;
    BoolExprSet softNonterm;
    solver->resetSolver();
    for (const auto &rel: todo) {
        auto it = res.find(rel);
        if (it == res.end()) {
            boolAbstractionMap[rel] = False;
            boolNontermAbstractionMap[rel] = False;
        } else {
            const std::vector<Entry> entries = it->second;
            Vars eVars;
            BoolExprSet abstraction;
            BoolExprSet nontermAbstraction;
            for (const Entry &e: entries) {
                BoolExpr entryVar = its.freshBoolVar();
                eVars.push_back(entryVar);
                if (!e.active) continue;
                abstraction.insert(entryVar);
                if (e.nonterm) nontermAbstraction.insert(entryVar);
                for (const Rel &dep: e.dependencies) {
                    solver->add((!entryVar) | edgeVars.at({rel, dep}));
                }
            }
            entryVars[rel] = eVars;
            BoolExpr res = buildOr(abstraction);
            boolAbstractionMap[rel] = res;
            soft.insert(res);
            BoolExpr ntRes = buildOr(nontermAbstraction);
            boolNontermAbstractionMap[rel] = ntRes;
            softNonterm.insert(ntRes);
        }
    }
    // forbids loops of length 2
    for (auto it1 = todo.begin(), end = todo.end(); it1 != end; ++it1) {
        for (auto it2 = it1; it2 != end; ++it2) {
            if (it1 == it2) continue;
                solver->add((!edgeVars.at({*it1, *it2})) | (!edgeVars.at({*it2, *it1})));
        }
    }
    std::vector<AccelerationProblem::Result> ret;
    bool positiveCost = Smt::isImplication(guard, buildLit(cost > 0), its);
    if (positiveCost) {
        solver->push();
        Smt::Result satRes = checkSat(edgeVars, boolNontermAbstractionMap);
        if (satRes == Smt::Sat) {
            option<AccelerationProblem::Result> res = enlargeSolutionAndGetRes(entryVars, edgeVars, softNonterm);
            if (res) {
                ret.push_back(*res);
            }
        }
    }
    if (closed) {
        solver->popAll();
        Smt::Result satRes = checkSat(edgeVars, boolAbstractionMap);
        if (satRes == Smt::Sat) {
            option<AccelerationProblem::Result> res = enlargeSolutionAndGetRes(entryVars, edgeVars, soft);
            if (res) {
                res->witnessesNonterm &= positiveCost;
                ret.push_back(*res);
            }
        }
    }
    return ret;
}

std::pair<BoolExpr, bool> AccelerationProblem::buildRes(const Model &model, const std::map<Rel, std::vector<BoolExpr>> &entryVars) {
    RelMap<BoolExpr> map;
    bool nonterm = true;
    RelMap<unsigned int> solution;
    for (const Rel &rel: todo) {
        map[rel] = False;
        if (res.count(rel) > 0) {
            std::vector<Entry> entries = res.at(rel);
            unsigned int eVarIdx = 0;
            const std::vector<BoolExpr> &eVars = entryVars.at(rel);
            for (auto eIt = entries.begin(), eEnd = entries.end(); eIt != eEnd; ++eIt, ++eVarIdx) {
                int id = eVars[eVarIdx]->getConst().get();
                if (model.contains(id) && model.get(id)) {
                    map[rel] = eIt->formula;
                    nonterm &= eIt->nonterm;
                    solution[rel] = eVarIdx;
                    break;
                }
            }
        }
    }
    BoolExpr ret = guard->replaceRels(map);
    if (!nonterm) {
        ret = (ret & (n >= validityBound));
    }
    proof.newline();
    proof.append("solution:");
    for (const auto &e: solution) {
        std::stringstream ss;
        ss << e.first << ": " << e.second;
        proof.append(ss);
    }
    proof.newline();
    proof.append(std::stringstream() << "resulting guard: " << ret);
    if (nonterm) {
        proof.newline();
        proof.append("resulting guard is a recurrent set");
    }
    return {ret, nonterm};
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
