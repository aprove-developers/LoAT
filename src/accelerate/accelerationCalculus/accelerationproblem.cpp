#include "accelerationproblem.hpp"
#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"

AccelerationProblem::AccelerationProblem(
        const BoolExpr guard,
        const Subs &up,
        option<const Subs&> closed,
        const Expr &cost,
        const Expr &iteratedCost,
        const Var &n,
        const unsigned int validityBound,
        VariableManager &varMan): todo(guard->lits()), up(up), closed(closed), cost(cost), iteratedCost(iteratedCost), n(n), guard(guard), validityBound(validityBound), varMan(varMan) {
    std::vector<Subs> subs = closed.map([&up](auto const &closed){return std::vector<Subs>{up, closed};}).get_value_or({up});
    Smt::Logic logic = Smt::chooseLogic<RelSet, Subs>({todo}, subs);
    this->solver = SmtFactory::modelBuildingSolver(logic, varMan);
    this->proof.append(std::stringstream() << "accelerating " << guard << " wrt. " << up);
}

option<AccelerationProblem> AccelerationProblem::init(const LinearRule &r, VariableManager &varMan) {
    const Var &n = varMan.addFreshTemporaryVariable("n");
    const option<Recurrence::Result> &res = Recurrence::iterateRule(varMan, r, n);
    if (res) {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        option<const Subs&>(res->update),
                        r.getCost(),
                        res->cost,
                        n,
                        res->validityBound,
                        varMan)};
    } else {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        option<const Subs&>(),
                        r.getCost(),
                        r.getCost(),
                        n,
                        0,
                        varMan)};
    }
}

RelSet AccelerationProblem::findConsistentSubset(const BoolExpr e) const {
    solver->resetSolver();
    solver->add(e);
    RelSet res;
    if (solver->check() == Smt::Sat) {
        const Subs &model = solver->model().toSubs();
        for (const Rel &rel: todo) {
            if (rel.subs(model).isTriviallyTrue()) {
                res.insert(rel);
            }
        }
    }
    return res;
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

void AccelerationProblem::monotonicity() {
    if (closed) {
        for (const Rel &rel: todo) {
            const auto it = res.find(rel);
            auto no_deps = [](auto const &e){return e.dependencies.empty();};
            if (it != res.end() && std::any_of(it->second.begin(), it->second.end(), no_deps)) {
                continue;
            }
            const Rel &updated = rel.subs(up);
            RelSet premise = findConsistentSubset(guard & rel & updated);
            if (!premise.empty()) {
                BoolExprSet assumptions;
                BoolExprSet deps;
                premise.erase(rel);
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
                    const BoolExpr newGuard = buildAnd(dependencies) & rel.subs(closed.get()).subs(Subs(n, n-1));
                    solver->resetSolver();
                    solver->add(newGuard);
                    solver->add(n >= validityBound);
                    if (solver->check() == Smt::Sat) {
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
                        }
                    }
                }
            }
        }
    }
}

void AccelerationProblem::recurrence() {
    for (const Rel &rel: todo) {
        const Rel updated = rel.subs(up);
        RelSet premise = findConsistentSubset(guard & rel & updated);
        if (!premise.empty()) {
            BoolExprSet deps;
            BoolExprSet assumptions;
            for (const Rel &p: premise) {
                const BoolExpr b = buildLit(p);
                assumptions.insert(b);
                deps.insert(b);
            }
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
                }
            }
        }
    }
}

void AccelerationProblem::eventualWeakDecrease() {
    if (closed) {
        for (const Rel &rel: todo) {
            const auto it = res.find(rel);
            auto no_deps = [](auto const &e){return e.dependencies.empty();};
            if (it != res.end() && std::any_of(it->second.begin(), it->second.end(), no_deps)) {
                continue;
            }
            const Expr &updated = rel.lhs().subs(up);
            const Rel &dec = rel.lhs() >= updated;
            const Rel &inc = updated < updated.subs(up);
            RelSet premise = findConsistentSubset(guard & dec & !inc);
            if (!premise.empty()) {
                premise.erase(rel);
                BoolExprSet assumptions;
                BoolExprSet deps;
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
                    const Rel &newCond = rel.subs(closed.get()).subs(Subs(n, n-1));
                    const BoolExpr newGuard = buildAnd(dependencies) & rel & newCond;
                    solver->resetSolver();
                    solver->add(newGuard);
                    solver->add(n >= validityBound);
                    if (solver->check() == Smt::Sat) {
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
                        }
                    }
                }
            }
        }
    }
}

void AccelerationProblem::eventualWeakIncrease() {
    for (const Rel &rel: todo) {
        const auto it = res.find(rel);
        auto no_deps = [](auto const &e){return e.nonterm && e.dependencies.empty();};
        if (it != res.end() && std::any_of(it->second.begin(), it->second.end(), no_deps)) {
            continue;
        }
        const Expr &updated = rel.lhs().subs(up);
        const Rel &inc = rel.lhs() <= updated;
        const Rel &dec = updated > updated.subs(up);
        RelSet premise = findConsistentSubset(guard & inc & !dec);
        if (!premise.empty()) {
            premise.erase(rel);
            BoolExprSet assumptions;
            BoolExprSet deps;
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
                const BoolExpr newGuard = buildAnd(dependencies) & rel & inc;
                solver->resetSolver();
                solver->add(newGuard);
                if (solver->check() == Smt::Sat) {
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
                    }
                }
            }
        }
    }
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

std::vector<AccelerationProblem::Result> AccelerationProblem::computeRes() {
    recurrence();
    monotonicity();
    eventualWeakDecrease();
    eventualWeakIncrease();
    using Edge = std::pair<Rel, Rel>;
    using Vars = std::vector<BoolExpr>;
    std::map<Edge, BoolExpr> edgeVars;
    std::map<Rel, Vars> entryVars;
    Vars soft;
    for (const Rel &rel1: todo) {
        for (const Rel &rel2: todo) {
            edgeVars[{rel1, rel2}] = varMan.freshBoolVar();
        }
    }
    // if an entry is enabled, then the edges corresponding to its dependencies have to be enabled.
    // maps every constraint to it's 'boolean abstraction'
    // which states that one of the entries corresponding to the constraint needs to be enabled
    RelMap<BoolExpr> boolAbstractionMap;
    RelMap<BoolExpr> boolNontermAbstractionMap;
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
                BoolExpr entryVar = varMan.freshBoolVar();
                eVars.push_back(entryVar);
                soft.push_back(entryVar);
                if (!e.active) continue;
                abstraction.insert(entryVar);
                if (e.nonterm) nontermAbstraction.insert(entryVar);
                for (const Rel &dep: e.dependencies) {
                    solver->add((!entryVar) | edgeVars.at({rel, dep}));
                }
            }
            entryVars[rel] = eVars;
            boolAbstractionMap[rel] = buildOr(abstraction);
            boolNontermAbstractionMap[rel] = buildOr(nontermAbstraction);
        }
    }
    // forbids loops of length 2
    for (auto it1 = todo.begin(), end = todo.end(); it1 != end; ++it1) {
        for (auto it2 = it1; it2 != end; ++it2) {
            if (it1 == it2) continue;
                solver->add((!edgeVars.at({*it1, *it2})) | (!edgeVars.at({*it2, *it1})));
        }
    }
    solver->push();
    BoolExpr boolNontermAbstraction = guard->replaceRels(boolNontermAbstractionMap);
    solver->add(boolNontermAbstraction);
    Smt::Result satRes = solver->check();
    std::vector<AccelerationProblem::Result> ret;
    if (satRes == Smt::Sat && checkCycle(edgeVars)) {
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
        satRes = solver->check();
    }
    if (satRes == Smt::Sat && Smt::isImplication(guard, buildLit(cost > 0), varMan)) {
        const auto p = buildRes(solver->model(), entryVars);
        const BoolExpr& newGuard = p.first;
        assert(p.second); // p.second = true means we've proven non-termination
        // TODO it would be better to encode satisfiability of the resulting guard in the constraint system
        if (Smt::check(newGuard, varMan) == Smt::Sat) {
            ret.push_back({newGuard, true});
        }
    }
    if (closed) {
        if (satRes != Smt::Sat) {
            solver->pop();
            BoolExpr boolAbstraction = guard->replaceRels(boolAbstractionMap);
            solver->add(boolAbstraction);
            satRes = solver->check();
            if (satRes == Smt::Sat && checkCycle(edgeVars)) {
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
                satRes = solver->check();
            }
        }
        if (satRes == Smt::Sat) {
            Model model = solver->model();
            for (const BoolExpr &s: soft) {
                solver->push();
                solver->add(s);
                satRes = solver->check();
                if (satRes == Smt::Sat) {
                    model = solver->model();
                } else {
                    solver->pop();
                }
            }
            const auto p = buildRes(model, entryVars);
            const BoolExpr& newGuard = p.first;
            bool nonterm = p.second;
            // TODO it would be better to encode satisfiability of the resulting guard in the constraint system
            if (Smt::check(newGuard, varMan) == Smt::Sat) {
                ret.push_back({newGuard, nonterm});
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

