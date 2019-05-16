/*  This file is part of LoAT.
 *  Copyright (c) 2019 Florian Frohn
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#include "../its/rule.hpp"
#include "../its/itsproblem.hpp"
#include "../expr/guardtoolbox.hpp"
#include "rulecontextbuilder.hpp"
#include "strengthener.hpp"

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

    const RuleContext Self::build() const {
        const std::vector<GiNaC::exmap> &updates = computeUpdates();
        const std::vector<Rule> &predecessors = computePredecessors();
        const std::vector<GuardList> preconditions = buildPreconditions(predecessors);
        return RuleContext(rule, updates, preconditions, its);
    }

}
