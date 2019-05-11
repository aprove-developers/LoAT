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

#include "../../util/timing.hpp"

#include <purrs.hh>

using namespace std;
namespace Purrs = Parma_Recurrence_Relation_Solver;



Recurrence::Recurrence(const VarMan &varMan, const std::vector<VariableIdx> &dependencyOrder)
    : varMan(varMan),
      ginacN(Purrs::Expr(Purrs::Recurrence::n).toGiNaC()),
      dependencyOrder(dependencyOrder)
{}


option<Recurrence::RecurrenceSolution> Recurrence::findUpdateRecurrence(const Expression &updateRhs, ExprSymbol updateLhs) {
    Timing::Scope timer(Timing::Purrs);

    Expression last = Purrs::x(Purrs::Recurrence::n - 1).toGiNaC();
    Purrs::Expr rhs = Purrs::Expr::fromGiNaC(updateRhs.subs(updatePreRecurrences).subs(updateLhs == last));
    Purrs::Expr exact;

    option<Expression> result0;
    option<Expression> result1;
    Purrs::Recurrence rec(rhs);
    Purrs::Recurrence::Solver_Status res = Purrs::Recurrence::Solver_Status::TOO_COMPLEX;
    try {
        rec.set_initial_conditions({ {0, Purrs::Expr::fromGiNaC(updateLhs)} });
        res = rec.compute_exact_solution();
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugPurrs("Purrs failed on x(n) = " << rhs << " with initial x(0)=" << updateLhs << " for updated variable " << updateLhs);
    }
    if (res == Purrs::Recurrence::SUCCESS) {
        rec.exact_solution(exact);
        result0 = exact.toGiNaC();
        if (result0.get().isPolynomial()) {
            return {{.res=result0.get(), .validityBound=0}};
        }
    }
    res = Purrs::Recurrence::Solver_Status::TOO_COMPLEX;
    try {
        rec.set_initial_conditions({ {1, Purrs::Expr::fromGiNaC(updateRhs)} });
        res = rec.compute_exact_solution();
    } catch (...) {
        debugPurrs("Purrs failed on x(n) = " << rhs << " with initial x(1)=" << updateRhs << " for updated variable " << updateLhs);
    }
    if (res == Purrs::Recurrence::SUCCESS) {
        rec.exact_solution(exact);
        result1 = exact.toGiNaC();
        if (!result0 || result1.get().isPolynomial()) {
            return {{.res=result1.get(), .validityBound=1}};;
        } else {
            return {{.res=result0.get(), .validityBound=0}};
        }
    }

    return {};
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


option<Recurrence::RecurrenceSystemSolution> Recurrence::iterateUpdate(const UpdateMap &update, const Expression &meterfunc) {
    assert(dependencyOrder.size() == update.size());
    UpdateMap newUpdate;

    //in the given order try to solve the recurrence for every updated variable
    unsigned int validityBound = 0;
    for (VariableIdx vi : dependencyOrder) {
        ExprSymbol target = varMan.getVarSymbol(vi);

        const Expression &rhs = update.at(vi);
        const ExprSymbolSet &vars = rhs.getVariables();
        option<Recurrence::RecurrenceSolution> updateRec = findUpdateRecurrence(rhs, target);
        if (!updateRec) {
            return {};
        }

        validityBound = max(validityBound, updateRec.get().validityBound);

        //remember this recurrence to replace vi in the updates depending on vi
        //note that updates need the value at n-1, e.g. x(n) = x(n-1) + vi(n-1) for the update x=x+vi
        updatePreRecurrences[target] = updateRec.get().res.subs(ginacN == ginacN-1);

        //calculate the final update using the loop's runtime
        newUpdate[vi] = updateRec.get().res.subs(ginacN == meterfunc);
    }

    return {{.update=newUpdate, .validityBound=validityBound}};
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


option<unsigned int> Recurrence::iterateAll(UpdateMap &update, Expression &cost, const Expression &metering) {
    auto newUpdate = iterateUpdate(update, metering);
    if (!newUpdate) {
        debugPurrs("calcIterated: failed to calculate update recurrence");
        return {};
    }

    auto newCost = iterateCost(cost, metering);
    if (!newCost) {
        debugPurrs("calcIterated: failed to calculate cost recurrence");
        return {};
    }

    update.swap(newUpdate.get().update);
    cost.swap(newCost.get());
    return newUpdate.get().validityBound;
}


option<unsigned int> Recurrence::iterateRule(const VarMan &varMan, LinearRule &rule, const Expression &metering) {
    // This may modify the rule's guard and update
    auto order = DependencyOrder::findOrderWithHeuristic(varMan, rule.getUpdateMut(), rule.getGuardMut());
    if (!order) {
        debugPurrs("iterateRule: failed to find a dependency order");
        return {};
    }

    Recurrence rec(varMan, order.get());
    return rec.iterateAll(rule.getUpdateMut(), rule.getCostMut(), metering);
}


option<unsigned int> Recurrence::iterateUpdateAndCost(const VarMan &varMan, UpdateMap &update, Expression &cost, GuardList &guard, const Expression &N) {
    auto order = DependencyOrder::findOrderWithHeuristic(varMan, update, guard);
    if (!order) {
        debugPurrs("iterateUpdateAndCost: failed to find a dependency order");
        return {};
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
    unsigned int validityBound = 0;
    for (const UpdateMap &up: updates) {
        const option<IteratedUpdates> it = iterateUpdate(varMan, up, n);
        if (it) {
            iteratedUpdates.insert(iteratedUpdates.end(), it.get().updates.begin(), it.get().updates.end());
            refinement.insert(refinement.end(), it.get().refinement.begin(), it.get().refinement.end());
            validityBound = max(validityBound, it->validityBound);
        } else {
            return {};
        }
    }
    return {{.updates=std::move(iteratedUpdates), .refinement=std::move(refinement), .validityBound=validityBound}};
}

const option<Recurrence::IteratedUpdates> Recurrence::iterateUpdate(
        const VariableManager &varMan,
        const UpdateMap &update,
        const ExprSymbol &n) {
    GuardList refinement;
    UpdateMap refinedUpdate = update;
    auto order = DependencyOrder::findOrderWithHeuristic(varMan, refinedUpdate, refinement);
    Recurrence rec(varMan, order.get());
    const option<RecurrenceSystemSolution> &iteratedUpdate = rec.iterateUpdate(refinedUpdate, n);
    if (iteratedUpdate) {
        return {{
            .updates={iteratedUpdate.get().update},
            .refinement=std::move(refinement),
            .validityBound=iteratedUpdate.get().validityBound
        }};
    } else {
        return {};
    }
}
