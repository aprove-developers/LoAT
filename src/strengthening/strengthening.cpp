//
// Created by ffrohn on 2/18/19.
//

#include <expr/relation.h>
#include <z3/z3context.h>
#include <z3/z3solver.h>
#include "strengthening.h"

typedef StrengtheningTypes Types;
typedef Strengthening Self;

const std::vector<Rule> Self::apply(const Rule &r, ITSProblem &its) {
    return Strengthening(r, its).apply();
}

Self::Strengthening(const Rule &r, ITSProblem &its):
        predecessors(computePredecessors(r, its)), r(r), varMan(its), updates(computeUpdates(r, its)) { }

const std::vector<Rule> Self::computePredecessors(const Rule &r, ITSProblem &its) {
    std::set <TransIdx> predecessorIndices = its.getTransitionsTo(r.getLhsLoc());
    std::set <TransIdx> successorIndices = its.getTransitionsFrom(r.getLhsLoc());
    std::vector <Rule> predecessors;
    for (const TransIdx &i: predecessorIndices) {
        if (successorIndices.count(i) == 0) {
            predecessors.push_back(its.getRule(i));
        }
    }
    return predecessors;
}

const std::vector<GiNaC::exmap> Self::computeUpdates(const Rule &r, VariableManager &varMan) {
    std::vector<GiNaC::exmap> res;
    for (const RuleRhs &rhs: r.getRhss()) {
        res.push_back(rhs.getUpdate().toSubstitution(varMan));
    }
    return res;
}

const std::vector<Rule> Self::apply() const {
    std::vector<Rule> res;
    if (r.isSimpleLoop()) {
        GuardList constraints;
        for (const Expression &e: r.getGuard()) {
            if (Relation::isLinearEquality(e)) {
                constraints.emplace_back(e.lhs() <= e.rhs());
                constraints.emplace_back(e.rhs() <= e.lhs());
            } else if (Relation::isLinearInequality(e)) {
                constraints.push_back(e);
            }
        }
        const Types::Result inv = splitInvariants(constraints);
        const Types::Result mon = splitMonotonicConstraints(inv.solved, inv.failed);
        if (!mon.failed.empty()) {
            const std::vector<GuardList> &preconditions = buildPreconditions();
            const option<Types::Invariants> &newInv = tryToForceInvariance(mon.failed, preconditions);
            if (newInv) {
                GuardList newGuard(r.getGuard());
                for (const Expression &g: newInv.get().invariants) {
                    newGuard.push_back(g);
                }
                GuardList pseudoInvariantsValid(newGuard);
                for (const Expression &g: newInv.get().pseudoInvariants) {
                    pseudoInvariantsValid.push_back(g);
                }
                res.emplace_back(Rule(RuleLhs(r.getLhsLoc(), pseudoInvariantsValid, r.getCost()), r.getRhss()));
                for (const Expression &e: newInv.get().pseudoInvariants) {
                    GuardList pseudoInvariantInvalid(newGuard);
                    assert(Relation::isInequality(e));
                    const Expression &negated = Relation::negateLessEqInequality(Relation::toLessEq(e));
                    debugTest("deduced pseudo-invariant " << e << ", also trying " << negated);
                    pseudoInvariantInvalid.push_back(negated);
                    res.emplace_back(Rule(RuleLhs(r.getLhsLoc(), pseudoInvariantInvalid, r.getCost()), r.getRhss()));
                }
            }
        }
    }
    return res;
}

const Types::Result Self::splitInvariants(const GuardList &constraints) const {
    Z3Context context;
    Z3Solver solver(context);
    for (const Expression &g: r.getGuard()) {
        solver.add(g.toZ3(context));
    }
    Types::Result res;
    for (const Expression &g: constraints) {
        bool isInvariant = true;
        for (const GiNaC::exmap &up: updates) {
            Expression conclusionExp = g;
            conclusionExp.applySubs(up);
            const z3::expr &conclusion = conclusionExp.toZ3(context);
            solver.push();
            solver.add(!conclusion);
            const z3::check_result &z3Res = solver.check();
            solver.pop();
            if (z3Res != z3::check_result::unsat) {
                isInvariant = false;
                break;
            }
        }
        if (isInvariant) {
            res.solved.push_back(g);
        } else {
            res.failed.push_back(g);
        }
    }
    return res;
}

const Types::Result Self::splitMonotonicConstraints(
        const GuardList &invariants,
        const GuardList &nonInvariants) const {
    Z3Context context;
    Z3Solver solver(context);
    for (const Expression &g: invariants) {
        solver.add(g.toZ3(context));
    }
    Types::Result res;
    for (const Expression &e: nonInvariants) {
        res.failed.push_back(e);
    }
    for (const GiNaC::exmap &up: updates) {
        solver.push();
        for (Expression g: r.getGuard()) {
            g.applySubs(up);
            solver.add(g.toZ3(context));
        }
        auto g = res.solved.begin();
        while (g != res.solved.end()) {
            solver.push();
            solver.add(!(*g).toZ3(context));
            const z3::check_result &z3Res = solver.check();
            solver.pop();
            if (z3Res == z3::check_result::unsat) {
                g++;
            } else {
                res.failed.push_back(*g);
                g = res.solved.erase(g);
            }
        }
        solver.pop();
    }
    return res;
}

const std::vector<GuardList> Self::buildPreconditions() const {
    std::vector<GuardList> res;
    GiNaC::exmap tmpVarRenaming;
    for (const VariableIdx &i: varMan.getTempVars()) {
        const ExprSymbol &x = varMan.getVarSymbol(i);
        tmpVarRenaming[x] = varMan.getVarSymbol(varMan.addFreshVariable(x.get_name()));
    }
    for (const Rule &pred: predecessors) {
        if (pred.getGuard() == r.getGuard()) {
            continue;
        }
        for (const RuleRhs &rhs: pred.getRhss()) {
            if (rhs.getLoc() == r.getLhsLoc()) {
                const GiNaC::exmap &up = rhs.getUpdate().toSubstitution(varMan);
                GuardList pre;
                for (Expression g: pred.getGuard()) {
                    g.applySubs(up);
                    g.applySubs(tmpVarRenaming);
                    pre.push_back(g);
                }
                for (const auto &p: rhs.getUpdate()) {
                    const ExprSymbol &var = varMan.getVarSymbol(p.first);
                    const Expression &varUpdate = p.second;
                    Expression updatedVarUpdate = varUpdate;
                    updatedVarUpdate.applySubs(up);
                    if (varUpdate == updatedVarUpdate) {
                        updatedVarUpdate.applySubs(tmpVarRenaming);
                        pre.push_back(var == updatedVarUpdate);
                    }
                }
                res.push_back(pre);
            }
        }
    }
    return res;
}

const ExprSymbolSet Self::findRelevantVariables(const Expression &c) const {
    std::set<VariableIdx> res;
    // Add all variables appearing in the guard
    const ExprSymbolSet &guardVariables = c.getVariables();
    for (const ExprSymbol &sym : guardVariables) {
        res.insert(varMan.getVarIdx(sym));
    }
    // Compute the closure of res under all updates and the guard
    std::set<VariableIdx> todo = res;
    while (!todo.empty()) {
        ExprSymbolSet next;
        for (const VariableIdx &var : todo) {
            for (const RuleRhs &rhs: r.getRhss()) {
                const UpdateMap &update = rhs.getUpdate();
                auto it = update.find(var);
                if (it != update.end()) {
                    const ExprSymbolSet &rhsVars = it->second.getVariables();
                    next.insert(rhsVars.begin(), rhsVars.end());
                }
            }
            for (const Expression &g: r.getGuard()) {
                const ExprSymbolSet &gVars = g.getVariables();
                if (gVars.find(varMan.getVarSymbol(var)) != gVars.end()) {
                    next.insert(gVars.begin(), gVars.end());
                }
            }
        }
        todo.clear();
        for (const ExprSymbol &sym : next) {
            const VariableIdx &var = varMan.getVarIdx(sym);
            if (res.count(var) == 0) {
                todo.insert(var);
            }
        }
        // collect all variables from every iteration
        res.insert(todo.begin(), todo.end());
    }
    ExprSymbolSet symbols;
    for (const VariableIdx &x: res) {
        symbols.insert(varMan.getVarSymbol(x));
    }
    return symbols;
}

const GuardList Self::findRelevantConstraints(const GuardList &guard, const ExprSymbolSet &vars) const {
    GuardList relevantConstraints;
    for (const Expression &e: guard) {
        for (const ExprSymbol &var: vars) {
            if (e.getVariables().count(var) > 0) {
                relevantConstraints.push_back(e);
                break;
            }
        }
    }
    return relevantConstraints;
}

const Types::Template Self::buildTemplate(const ExprSymbolSet &vars) const {
    ExprSymbolSet params;
    const ExprSymbol &c0 = varMan.getVarSymbol(varMan.addFreshVariable("c0"));
    params.insert(c0);
    Expression res = c0;
    for (const ExprSymbol &x: vars) {
        const ExprSymbol &param = varMan.getVarSymbol(varMan.addFreshVariable("c"));
        params.insert(param);
        res = res + (x * param);
    }
    return Types::Template(std::move(res), std::move(params));
}

const option<Types::Invariants> Self::tryToForceInvariance(
        const GuardList &todo,
        const std::vector<GuardList> &preconditions) const {
    Z3Context context;
    ExprSymbolSet allRelevantVariables;
    std::vector<Expression> templates;
    ExprSymbolSet templateParams;
    for (const Expression &g: todo) {
        const ExprSymbolSet &varSymbols = findRelevantVariables(g);
        allRelevantVariables.insert(varSymbols.begin(), varSymbols.end());
        const Types::Template &t = buildTemplate(varSymbols);
        templates.emplace_back(t.t <= 0);
        templateParams.insert(t.params.begin(), t.params.end());
    }
    const GuardList &relevantConstraints = findRelevantConstraints(r.getGuard(), allRelevantVariables);
    return InvarianceStrengthening(
            templates,
            templateParams,
            relevantConstraints,
            allRelevantVariables,
            updates,
            preconditions,
            todo,
            varMan).apply();
}
