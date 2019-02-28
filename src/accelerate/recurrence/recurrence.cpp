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

#include "recurrence.h"
#include "dependencyorder.h"

#include "util/timing.h"

#include <purrs.hh>

using namespace std;
namespace Purrs = Parma_Recurrence_Relation_Solver;



Recurrence::Recurrence(const VarMan &varMan, const std::vector<VariableIdx> &dependencyOrder)
    : varMan(varMan),
      ginacN(Purrs::Expr(Purrs::Recurrence::n).toGiNaC()),
      dependencyOrder(dependencyOrder)
{}


option<Expression> Recurrence::findUpdateRecurrence(const Expression &updateRhs, ExprSymbol updateLhs) {
    Timing::Scope timer(Timing::Purrs);

    Expression last = Purrs::x(Purrs::Recurrence::n - 1).toGiNaC();
    Purrs::Expr rhs = Purrs::Expr::fromGiNaC(updateRhs.subs(updatePreRecurrences).subs(updateLhs == last));
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {1, Purrs::Expr::fromGiNaC(updateRhs)} });

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            return {};
        }
        rec.exact_solution(exact);
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugPurrs("Purrs failed on x(n) = " << rhs << " with initial x(1)=" << updateRhs << " for updated variable " << updateLhs);
        return {};
    }

    Expression result = exact.toGiNaC();
    return result;
}


option<Expression> Recurrence::findCostRecurrence(Expression cost) {
    Timing::Scope timer(Timing::Purrs);
    cost = cost.subs(updatePreRecurrences); //replace variables by their recurrence equations

    //Example: if cost = y, the result is x(n) = x(n-1) + y(n-1), with x(0) = 0
    Purrs::Expr rhs = Purrs::x(Purrs::Recurrence::n - 1) + Purrs::Expr::fromGiNaC(cost);
    Purrs::Expr sol;

    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {0, 0} }); // 0 iterations have 0 costs

        debugPurrs("COST REC: " << rhs);

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            // try lower bound as fallback, since it is sound to under-approximate costs
            res = rec.compute_lower_bound();
            if (res != Purrs::Recurrence::SUCCESS) {
                debugPurrs("Purrs failed on x(n) = " << rhs << " with initial x(0)=0 for cost" << cost);
                return {};
            } else {
                rec.lower_bound(sol);
            }
        } else {
            rec.exact_solution(sol);
        }
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugPurrs("Purrs failed (exception) on x(n) = " << rhs << " with initial x(0)=0 for cost" << cost);
        return {};
    }

    Expression result = sol.toGiNaC();
    return result;
}


option<UpdateMap> Recurrence::iterateUpdate(const UpdateMap &update, const Expression &meterfunc) {
    assert(dependencyOrder.size() == update.size());
    UpdateMap newUpdate;

    //in the given order try to solve the recurrence for every updated variable
    for (VariableIdx vi : dependencyOrder) {
        ExprSymbol target = varMan.getVarSymbol(vi);

        auto updateRec = findUpdateRecurrence(update.at(vi),target);
        if (!updateRec) {
            return {};
        }

        //remember this recurrence to replace vi in the updates depending on vi
        //note that updates need the value at n-1, e.g. x(n) = x(n-1) + vi(n-1) for the update x=x+vi
        updatePreRecurrences[target] = updateRec.get().subs(ginacN == ginacN-1);

        //calculate the final update using the loop's runtime
        newUpdate[vi] = updateRec.get().subs(ginacN == meterfunc);
    }

    return newUpdate;
}


option<Expression> Recurrence::iterateCost(const Expression &cost, const Expression &meterfunc) {
    //calculate the new cost sum
    auto costRec = findCostRecurrence(cost);
    if (costRec) {
        Expression res = costRec.get().subs(ginacN == meterfunc);
        return res;
    }
    return {};
}


bool Recurrence::iterateAll(UpdateMap &update, Expression &cost, const Expression &metering) {
    auto newUpdate = iterateUpdate(update, metering);
    if (!newUpdate) {
        debugPurrs("calcIterated: failed to calculate update recurrence");
        return false;
    }

    auto newCost = iterateCost(cost, metering);
    if (!newCost) {
        debugPurrs("calcIterated: failed to calculate cost recurrence");
        return false;
    }

    update.swap(newUpdate.get());
    cost.swap(newCost.get());
    return true;
}


bool Recurrence::iterateRule(const VarMan &varMan, LinearRule &rule, const Expression &metering) {
    // This may modify the rule's guard and update
    auto order = DependencyOrder::findOrderWithHeuristic(varMan, rule.getUpdateMut(), rule.getGuardMut());
    if (!order) {
        debugPurrs("iterateRule: failed to find a dependency order");
        return false;
    }

    Recurrence rec(varMan, order.get());
    return rec.iterateAll(rule.getUpdateMut(), rule.getCostMut(), metering);
}


bool Recurrence::iterateUpdateAndCost(const VarMan &varMan, UpdateMap &update, Expression &cost, GuardList &guard, const Expression &N) {
    auto order = DependencyOrder::findOrderWithHeuristic(varMan, update, guard);
    if (!order) {
        debugPurrs("iterateUpdateAndCost: failed to find a dependency order");
        return false;
    }

    Recurrence rec(varMan, order.get());
    return rec.iterateAll(update, cost, N);
}

const option<Recurrence::IteratedUpdates> Recurrence::iterateUpdates(
        const VariableManager &varMan,
        const std::vector<UpdateMap> &updates,
        const ExprSymbol &n) {
    std::vector<UpdateMap> iteratedUpdates;
    GuardList refinement;
    for (const UpdateMap &up: updates) {
        const option<IteratedUpdates> it = iterateUpdate(varMan, up, n);
        if (it) {
            iteratedUpdates.insert(iteratedUpdates.end(), it.get().updates.begin(), it.get().updates.end());
            refinement.insert(refinement.end(), it.get().refinement.begin(), it.get().refinement.end());
        } else {
            return {};
        }
    }
    return {{.updates=std::move(iteratedUpdates), .refinement=std::move(refinement)}};
}

const option<Recurrence::IteratedUpdates> Recurrence::iterateUpdate(
        const VariableManager &varMan,
        const UpdateMap &update,
        const ExprSymbol &n) {
    GuardList refinement;
    UpdateMap refinedUpdate = update;
    auto order = DependencyOrder::findOrderWithHeuristic(varMan, refinedUpdate, refinement);
    Recurrence rec(varMan, order.get());
    const option<UpdateMap> &iteratedUpdate = rec.iterateUpdate(refinedUpdate, n);
    if (iteratedUpdate) {
        return {{.updates={iteratedUpdate.get()}, .refinement=std::move(refinement)}};
    } else {
        return {};
    }
}
