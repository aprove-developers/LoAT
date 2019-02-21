//
// Created by ffrohn on 2/21/19.
//

#include <its/rule.h>
#include <its/itsproblem.h>
#include "setup.h"
#include "types.h"
#include <z3context.h>
#include <z3/z3solver.h>
#include <expr/relation.h>

namespace strengthening {

    typedef Setup Self;

    Self::Setup(const Rule &rule, ITSProblem &its): rule(rule), its(its) { }

    const std::vector<Rule> Self::computePredecessors() const {
        std::set<TransIdx> predecessorIndices = its.getTransitionsTo(rule.getLhsLoc());
        std::set<TransIdx> successorIndices = its.getTransitionsFrom(rule.getLhsLoc());
        std::vector<Rule> predecessors;
        for (const TransIdx &i: predecessorIndices) {
            if (successorIndices.count(i) == 0) {
                predecessors.push_back(its.getRule(i));
            }
        }
        return predecessors;
    }

    const std::vector<GiNaC::exmap> Self::computeUpdates() const {
        std::vector<GiNaC::exmap> res;
        for (const RuleRhs &rhs: rule.getRhss()) {
            res.push_back(rhs.getUpdate().toSubstitution(its));
        }
        return res;
    }

    const GuardList Self::computeConstraints() const {
        GuardList constraints;
        for (const Expression &e: rule.getGuard()) {
            if (Relation::isLinearEquality(e)) {
                constraints.emplace_back(e.lhs() <= e.rhs());
                constraints.emplace_back(e.rhs() <= e.lhs());
            } else if (Relation::isLinearInequality(e)) {
                constraints.push_back(e);
            }
        }
        return constraints;
    }

    const Result Self::splitInvariants(const GuardList &constraints, const std::vector<GiNaC::exmap> &updates) const {
        Z3Context context;
        Z3Solver solver(context);
        for (const Expression &g: rule.getGuard()) {
            solver.add(g.toZ3(context));
        }
        Result res;
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

    const Result Self::splitMonotonicConstraints(
            const GuardList &invariants,
            const GuardList &nonInvariants,
            const std::vector<GiNaC::exmap> &updates) const {
        Z3Context context;
        Z3Solver solver(context);
        for (const Expression &g: invariants) {
            solver.add(g.toZ3(context));
        }
        Result res;
        res.solved.insert(res.solved.end(), nonInvariants.begin(), nonInvariants.end());
        for (const GiNaC::exmap &up: updates) {
            solver.push();
            for (Expression g: rule.getGuard()) {
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

    const std::vector<GuardList> Self::buildPreconditions(const std::vector<Rule> &predecessors) const {
        std::vector<GuardList> res;
        GiNaC::exmap tmpVarRenaming;
        for (const VariableIdx &i: its.getTempVars()) {
            const ExprSymbol &x = its.getVarSymbol(i);
            tmpVarRenaming[x] = its.getVarSymbol(its.addFreshVariable(x.get_name()));
        }
        for (const Rule &pred: predecessors) {
            if (pred.getGuard() == rule.getGuard()) {
                continue;
            }
            for (const RuleRhs &rhs: pred.getRhss()) {
                if (rhs.getLoc() == rule.getLhsLoc()) {
                    const GiNaC::exmap &up = rhs.getUpdate().toSubstitution(its);
                    GuardList pre;
                    for (Expression g: pred.getGuard()) {
                        g.applySubs(up);
                        g.applySubs(tmpVarRenaming);
                        pre.push_back(g);
                    }
                    for (const auto &p: rhs.getUpdate()) {
                        const ExprSymbol &var = its.getVarSymbol(p.first);
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

    const Context Self::setup() {
        const std::vector<GiNaC::exmap> &updates = computeUpdates();
        const GuardList &constraints = computeConstraints();
        const Result inv = splitInvariants(constraints, updates);
        const Result mon = splitMonotonicConstraints(inv.solved, inv.failed, updates);
        const std::vector<Rule> &predecessors = computePredecessors();
        const std::vector<GuardList> preconditions = buildPreconditions(predecessors);
        return Context(rule, updates, mon.solved, mon.failed, preconditions, its);
    }

}