//
// Created by ffrohn on 2/21/19.
//

#include <its/rule.h>
#include <its/itsproblem.h>
#include <expr/guardtoolbox.h>
#include "rulecontextbuilder.h"
#include "strengthener.h"

namespace strengthening {

    typedef RuleContextBuilder Self;

    const RuleContext Self::build(const Rule &rule, ITSProblem &its) {
        return RuleContextBuilder(rule, its).build();
    }

    Self::RuleContextBuilder(const Rule &rule, ITSProblem &its): rule(rule), its(its) { }

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

    const std::vector<Rule> Self::computeSuccessors() const {
        std::set<TransIdx> predecessorIndices = its.getTransitionsTo(rule.getLhsLoc());
        std::set<TransIdx> successorIndices = its.getTransitionsFrom(rule.getLhsLoc());
        std::vector<Rule> successors;
        for (const TransIdx &i: successorIndices) {
            if (predecessorIndices.count(i) == 0) {
                successors.push_back(its.getRule(i));
            }
        }
        return successors;
    }

    const std::vector<GiNaC::exmap> Self::computeUpdates() const {
        std::vector<GiNaC::exmap> res;
        for (const RuleRhs &rhs: rule.getRhss()) {
            res.push_back(rhs.getUpdate().toSubstitution(its));
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

    const std::vector<GuardList> Self::buildPostconditions(const std::vector<Rule> &successors) const {
        std::vector<GuardList> res;
        GiNaC::exmap tmpVarRenaming;
        for (const VariableIdx &i: its.getTempVars()) {
            const ExprSymbol &x = its.getVarSymbol(i);
            tmpVarRenaming[x] = its.getVarSymbol(its.addFreshVariable(x.get_name()));
        }
        for (const Rule &succ: successors) {
            if (succ.getGuard() == rule.getGuard()) {
                continue;
            }
            GuardList post;
            for (const Expression &g: succ.getGuard()) {
                if (!GuardToolbox::containsTempVar(its, g)) {
                    post.push_back(g);
                }
            }
            res.push_back(post);
        }
        return res;
    }

    const RuleContext Self::build() const {
        const std::vector<GiNaC::exmap> &updates = computeUpdates();
        const std::vector<Rule> &predecessors = computePredecessors();
        const std::vector<GuardList> preconditions = buildPreconditions(predecessors);
        const std::vector<Rule> &successors = computeSuccessors();
        const std::vector<GuardList> postconditions = buildPostconditions(predecessors);
        return RuleContext(rule, updates, preconditions, postconditions, its);
    }

}