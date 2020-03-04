/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
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

#include "recurrence.hpp"
#include "dependencyorder.hpp"

#include <purrs.hh>

using namespace std;
namespace Purrs = Parma_Recurrence_Relation_Solver;



Recurrence::Recurrence(const VarMan &varMan, const std::vector<VariableIdx> &dependencyOrder)
    : varMan(varMan),
      ginacN(Purrs::Expr(Purrs::Recurrence::n).toGiNaC()),
      dependencyOrder(dependencyOrder)
{}


option<Recurrence::RecurrenceSolution> Recurrence::findUpdateRecurrence(const Expression &updateRhs, ExprSymbol updateLhs, const std::map<VariableIdx, unsigned int> &validitybounds) {
    Expression last = Purrs::x(Purrs::Recurrence::n - 1).toGiNaC();
    Purrs::Expr rhs = Purrs::Expr::fromGiNaC(updateRhs.subs(updatePreRecurrences).subs(ExprMap(updateLhs, last)).ex);
    Purrs::Expr exact;

    const ExprSymbolSet &vars = updateRhs.getVariables();
    if (vars.find(updateLhs) == vars.end()) {
        unsigned int validitybound = 1;
        for (const ExprSymbol &ex: vars) {
            VariableIdx vi = varMan.getVarIdx(ex);
            if (validitybounds.find(vi) != validitybounds.end() && validitybounds.at(vi) + 1 > validitybound) {
                validitybound = validitybounds.at(vi) + 1;
            }
        }
        return {{.res=updateRhs.subs(updatePreRecurrences), .validityBound=validitybound}};
    }
    Purrs::Recurrence rec(rhs);
    Purrs::Recurrence::Solver_Status res = Purrs::Recurrence::Solver_Status::TOO_COMPLEX;
    try {
        rec.set_initial_conditions({ {0, Purrs::Expr::fromGiNaC(updateLhs)} });
        res = rec.compute_exact_solution();
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
    }
    if (res == Purrs::Recurrence::SUCCESS) {
        rec.exact_solution(exact);
        return {{.res=exact.toGiNaC(), .validityBound=0}};
    }
    return {};
}


option<Expression> Recurrence::findCostRecurrence(Expression cost) {
    cost = cost.subs(updatePreRecurrences); //replace variables by their recurrence equations

    //Example: if cost = y, the result is x(n) = x(n-1) + y(n-1), with x(0) = 0
    Purrs::Expr rhs = Purrs::x(Purrs::Recurrence::n - 1) + Purrs::Expr::fromGiNaC(cost.ex);
    Purrs::Expr sol;

    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {0, 0} }); // 0 iterations have 0 costs

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            // try lower bound as fallback, since it is sound to under-approximate costs
            res = rec.compute_lower_bound();
            if (res != Purrs::Recurrence::SUCCESS) {
                return {};
            } else {
                rec.lower_bound(sol);
            }
        } else {
            rec.exact_solution(sol);
        }
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        return {};
    }

    return {Expression(sol.toGiNaC())};
}


option<Recurrence::RecurrenceSystemSolution> Recurrence::iterateUpdate(const UpdateMap &update, const Expression &meterfunc) {
    assert(dependencyOrder.size() == update.size());
    UpdateMap newUpdate;

    //in the given order try to solve the recurrence for every updated variable
    unsigned int validityBound = 0;
    std::map<VariableIdx, unsigned int> validityBounds;
    for (VariableIdx vi : dependencyOrder) {
        ExprSymbol target = varMan.getVarSymbol(vi);

        const Expression &rhs = update.at(vi);
        option<Recurrence::RecurrenceSolution> updateRec = findUpdateRecurrence(rhs, target, validityBounds);
        if (!updateRec) {
            return {};
        }

        validityBounds[vi] = updateRec.get().validityBound;
        validityBound = max(validityBound, updateRec.get().validityBound);

        //remember this recurrence to replace vi in the updates depending on vi
        //note that updates need the value at n-1, e.g. x(n) = x(n-1) + vi(n-1) for the update x=x+vi
        updatePreRecurrences.put(target, updateRec.get().res.subs(ExprMap(ginacN, ginacN-1)));

        //calculate the final update using the loop's runtime
        newUpdate[vi] = updateRec.get().res.subs(ExprMap(ginacN, meterfunc));
    }

    return {{.update=newUpdate, .validityBound=validityBound}};
}


option<Expression> Recurrence::iterateCost(const Expression &cost, const Expression &meterfunc) {
    //calculate the new cost sum
    auto costRec = findCostRecurrence(cost);
    if (costRec) {
        Expression res = costRec.get().subs(ExprMap(ginacN, meterfunc));
        return {res};
    }
    return {};
}


option<Recurrence::Result> Recurrence::iterate(const UpdateMap &update, const Expression &cost, const Expression &metering) {
    auto newUpdate = iterateUpdate(update, metering);
    if (!newUpdate) {
        return {};
    }

    auto newCost = iterateCost(cost, metering);
    if (!newCost) {
        return {};
    }

    Recurrence::Result res;
    res.cost = newCost.get();
    res.update = newUpdate.get().update;
    res.validityBound = newUpdate.get().validityBound;
    return {res};
}


option<Recurrence::Result> Recurrence::iterateRule(const VarMan &varMan, const LinearRule &rule, const Expression &metering) {
    // This may modify the rule's guard and update
    auto order = DependencyOrder::findOrder(varMan, rule.getUpdate());
    if (!order) {
        return {};
    }

    Recurrence rec(varMan, order.get());
    return rec.iterate(rule.getUpdate(), rule.getCost(), metering);
}

