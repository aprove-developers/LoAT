#include "accelerationproblem.hpp"
#include "../../accelerate/recurrence/recurrence.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../sat/satfactory.hpp"

AccelerationProblem::AccelerationProblem(
        const BoolExpr &guard,
        const Subs &up,
        const Subs &closed,
        const Expr &cost,
        const Expr &iteratedCost,
        const Var &n,
        const uint validityBound,
        const VariableManager &varMan): todo(guard->lits()), up(up), closed(closed), cost(cost), iteratedCost(iteratedCost), n(n), guard(guard), validityBound(validityBound), varMan(varMan) {
    Smt::Logic logic = Smt::chooseLogic<RelSet, Subs>({todo}, {up, closed});
    this->solver = SmtFactory::modelBuildingSolver(logic, varMan);
    this->solver->enableUnsatCores();
    this->proof.append(std::stringstream() << "accelerating " << guard << " wrt. " << up);
}

option<AccelerationProblem> AccelerationProblem::init(const LinearRule &r, VariableManager &varMan) {
    const Var &n = varMan.addFreshTemporaryVariable("n");
    const option<Recurrence::Result> &res = Recurrence::iterateRule(varMan, r, n);
    if (res) {
        return {AccelerationProblem(
                        r.getGuard()->toG(),
                        r.getUpdate(),
                        res->update,
                        r.getCost(),
                        res->cost,
                        n,
                        res->validityBound,
                        varMan)};
    } else {
        return {};
    }
}

BoolExpr AccelerationProblem::getGuardWithout(const Rel &rel) {
    const auto &it = guardWithout.find(rel);
    if (it == guardWithout.end()) {
        const BoolExpr res = guard->removeRels({rel}).get();
        guardWithout[rel] = res;
        return res;
    } else {
        return it->second;
    }
}

RelSet AccelerationProblem::findConsistentSubset(const BoolExpr &e) const {
    solver->resetSolver();
    solver->add(e);
    RelSet res;
    if (solver->check() == Sat) {
        const Subs &model = solver->modelSubs();
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

option<uint> AccelerationProblem::store(const Rel &rel, const RelSet &deps, const BoolExpr &formula, bool nonterm) {
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
    for (const Rel &rel: todo) {
        const Rel &updated = rel.subs(up);
        RelSet premise = findConsistentSubset(guard & rel & updated);
        solver->resetSolver();
        if (!premise.empty()) {
            std::map<uint, BoolExpr> assumptions;
            premise.erase(rel);
            for (const Rel &p: premise) {
                assumptions.emplace(solver->add(p), buildLit(p));
            }
            solver->add(updated);
            solver->add(!rel);
            if (solver->check() == Unsat) {
                const std::vector<uint> core = solver->unsatCore();
                RelSet dependencies;
                for (uint i: core) {
                    if (assumptions.count(i) > 0) {
                        const RelSet &deps = assumptions[i]->lits();
                        dependencies.insert(deps.begin(), deps.end());
                    }
                }
                const BoolExpr newGuard = buildAnd(dependencies) & rel.subs(closed).subs(Subs(n, n-1));
                solver->resetSolver();
                solver->add(newGuard);
                solver->add(n >= validityBound);
                if (solver->check() == Sat) {
                    option<uint> idx = store(rel, dependencies, newGuard);
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

void AccelerationProblem::recurrence() {
    for (const Rel &rel: todo) {
        const Rel updated = rel.subs(up);
        RelSet premise = findConsistentSubset(guard & rel & updated);
        solver->resetSolver();
        if (!premise.empty()) {
            std::map<uint, BoolExpr> assumptions;
            for (const Rel &p: premise) {
                assumptions.emplace(solver->add(p), buildLit(p));
            }
            solver->add(!updated);
            if (solver->check() == Unsat) {
                const std::vector<uint> core = solver->unsatCore();
                RelSet dependencies;
                for (uint i: core) {
                    if (assumptions.count(i) > 0) {
                        const RelSet &deps = assumptions[i]->lits();
                        dependencies.insert(deps.begin(), deps.end());
                    }
                }
                dependencies.erase(rel);
                const BoolExpr newGuard = buildAnd(dependencies) & rel;
                option<uint> idx = store(rel, dependencies, newGuard, true);
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
    for (const Rel &rel: todo) {
        const Expr &updated = rel.lhs().subs(up);
        const Rel &dec = rel.lhs() >= updated;
        const Rel &inc = updated < updated.subs(up);
        RelSet premise = findConsistentSubset(guard & dec & !inc);
        solver->resetSolver();
        if (!premise.empty()) {
            premise.erase(rel);
            std::map<uint, BoolExpr> assumptions;
            for (const Rel &p: premise) {
                assumptions.emplace(solver->add(p), buildLit(p));
            }
            solver->add(dec);
            solver->add(inc);
            if (solver->check() == Unsat) {
                const std::vector<uint> core = solver->unsatCore();
                RelSet dependencies;
                for (uint i: core) {
                    if (assumptions.count(i) > 0) {
                        const RelSet &deps = assumptions[i]->lits();
                        dependencies.insert(deps.begin(), deps.end());
                    }
                }
                const Rel &newCond = rel.subs(closed).subs(Subs(n, n-1));
                const BoolExpr newGuard = buildAnd(dependencies) & rel & newCond;
                solver->resetSolver();
                solver->add(newGuard);
                solver->add(n >= validityBound);
                if (solver->check() == Sat) {
                    option<uint> idx = store(rel, dependencies, newGuard);
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

option<AccelerationProblem::Result> AccelerationProblem::computeRes() {
    recurrence();
    monotonicity();
    eventualWeakDecrease();
    if (std::any_of(todo.begin(), todo.end(), [&](const Rel &rel) {return res.count(rel) == 0;})) {
        return {};
    }
    using Edge = std::pair<Rel, Rel>;
    using Vars = std::vector<PropExpr>;
    std::map<Edge, PropExpr> edgeVars;
    std::map<Rel, Vars> entryVars;
    uint varId = 0;
    for (const Rel &rel1: todo) {
        for (const Rel &rel2: todo) {
            edgeVars[{rel1, rel2}] = PropLit::buildLit(++varId);
        }
    }
    // if an entry is enabled, then the edges corresponding to its dependencies have to be enabled.
    PropExprSet init;
    // maps every constraint to it's 'boolean abstraction'
    // which states that one of the entries corresponding to the constraint needs to be enabled
    RelMap<PropExpr> boolAbstractionMap;
    RelMap<PropExpr> boolNontermAbstractionMap;
    for (const auto &p: res) {
        const Rel &rel = p.first;
        const std::vector<Entry> entries = p.second;
        Vars eVars;
        PropExprSet abstraction;
        PropExprSet nontermAbstraction;
        for (const Entry &e: entries) {
            PropExpr entryVar = PropLit::buildLit(++varId);
            eVars.push_back(entryVar);
            if (!e.active) continue;
            abstraction.insert(entryVar);
            if (e.nonterm) nontermAbstraction.insert(entryVar);
            for (const Rel &dep: e.dependencies) {
                init.insert(PropJunction::buildOr({!entryVar, edgeVars.at({rel, dep})}));
            }
        }
        entryVars[rel] = eVars;
        boolAbstractionMap[rel] = PropJunction::buildOr(abstraction);
        boolNontermAbstractionMap[rel] = PropJunction::buildOr(nontermAbstraction);
    }
    // if a->b and b->c is enabled, then a->c needs to be enabled, too
    PropExprSet closure;
    for (const auto &p: edgeVars) {
        const Edge &edge = p.first;
        const Rel &start = edge.first;
        const Rel &join = edge.second;
        const PropExpr var1 = p.second;
        for (const Rel &target: todo) {
            if (target == join) continue;
            const PropExpr var2 = edgeVars.at({join, target});
            const PropExpr var3 = edgeVars.at({start, target});
            closure.insert(PropJunction::buildOr({!var1, !var2, var3}));
        }
    }
    // forbids self-loops (which suffices due to 'closure' above)
    PropExprSet acyclic;
    for (const Rel &rel: todo) {
        acyclic.insert(!edgeVars.at({rel, rel}));
    }
    std::unique_ptr<sat::Sat> sat = sat::SatFactory::solver();
    sat->add(PropJunction::buildAnd(init));
    sat->add(PropJunction::buildAnd(closure));
    sat->add(PropJunction::buildAnd(acyclic));
    sat->push();
    PropExpr boolNontermAbstraction = guard->replaceRels(boolNontermAbstractionMap);
    sat->add(boolNontermAbstraction);
    SatResult satRes = sat->check();
    if (satRes != Sat) {
        sat->pop();
        PropExpr boolAbstraction = guard->replaceRels(boolAbstractionMap);
        sat->add(boolAbstraction);
        satRes = sat->check();
    }
    if (satRes == Sat) {
        const sat::Model &model = sat->model();
        RelMap<BoolExpr> map;
        bool nonterm = true;
        RelMap<std::vector<uint>> solution;
        for (const Rel &rel: todo) {
            std::vector<BoolExpr> replacement;
            if (res.count(rel) > 0) {
                std::vector<uint> sol;
                std::vector<Entry> entries = res.at(rel);
                uint eVarIdx = 0;
                const std::vector<PropExpr> &eVars = entryVars.at(rel);
                for (auto eIt = entries.begin(), eEnd = entries.end(); eIt != eEnd; ++eIt, ++eVarIdx) {
                    int id = eVars[eVarIdx]->lit().get();
                    if (model.contains(id) && model.get(id)) {
                        replacement.push_back(eIt->formula);
                        nonterm &= eIt->nonterm;
                        sol.push_back(eVarIdx);
                    }
                }
                if (!sol.empty()) {
                    solution[rel] = sol;
                }
            }
            map[rel] = buildOr(replacement);
        }
        BoolExpr ret = guard->replaceRels(map);
        nonterm &= Smt::isImplication(guard, buildLit(cost > 0), varMan);
        if (!nonterm) {
            ret = (ret & (n >= validityBound));
        }
        proof.newline();
        proof.append("solution:");
        for (const auto &e: solution) {
            std::stringstream ss;
            ss << e.first << ": [";
            bool first = true;
            for (uint i: e.second) {
                if (first) {
                    first = false;
                } else {
                    ss << " ";
                }
                ss << i;
            }
            ss << "]";
            proof.append(ss);
        }
        proof.newline();
        proof.append(std::stringstream() << "resulting guard: " << ret);
        if (nonterm) {
            proof.newline();
            proof.append("resulting guard is a recurrent set");
        }
        return {{ret, nonterm}};
    } else {
        return {};
    }
}

Proof AccelerationProblem::getProof() const {
    return proof;
}

Expr AccelerationProblem::getAcceleratedCost() const {
    return iteratedCost;
}

Subs AccelerationProblem::getClosedForm() const {
    return closed;
}

Var AccelerationProblem::getIterationCounter() const {
    return n;
}

