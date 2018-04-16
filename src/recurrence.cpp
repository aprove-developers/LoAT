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

#include "flowgraph.h"
#include "util/timing.h"

#include <purrs.hh>

using namespace std;
namespace Purrs = Parma_Recurrence_Relation_Solver;



Recurrence::Recurrence(const ITRSProblem &itrs)
    : itrs(itrs), ginacN(Purrs::Expr(Purrs::Recurrence::n).toGiNaC())
{}


//NOTE: very stupid and not very efficient; better use a graph for this problem
vector<VariableIndex> Recurrence::dependencyOrder(UpdateMap &update) {
    vector<VariableIndex> ordering;
    set<VariableIndex> orderedVars;

    while (ordering.size() < update.size()) {
        bool changed = false;
        for (auto up : update) {
            if (orderedVars.count(up.first) > 0) continue;

            //check if all variables on update rhs are already processed
            for (const string &varname : up.second.getVariableNames()) {
                VariableIndex vi = itrs.getVarindex(varname);
                if (vi != up.first && update.count(vi) > 0 && orderedVars.count(vi) == 0)
                    goto skipUpdate;
            }

            orderedVars.insert(up.first);
            ordering.push_back(up.first);
            changed = true;

            skipUpdate: ;
        }

        if (changed) continue;

        //if not all dependencies could be resolved, try to add constraints to the guard to make things easier
        GiNaC::exmap subs;
        ExprSymbol target;
        bool has_target = false;
        for (auto up : update) {
            if (orderedVars.count(up.first) > 0) continue;
            if (!has_target) {
                target = itrs.getGinacSymbol(up.first);
                has_target = true;
            } else {
                addGuard.push_back(target == itrs.getGinacSymbol(up.first));
                subs[itrs.getGinacSymbol(up.first)] = target;
            }
        }
        //assume all remaining variables are equal
        for (auto &up : update) {
            up.second = up.second.subs(subs);
        }
    }
    return ordering;
}


//NOTE: variables in update must already be replaced by their recurrence (containing Purrs::Recurrence::n)
bool Recurrence::findUpdateRecurrence(Expression update, ExprSymbol target, Expression &result) {
    Timing::Scope timer(Timing::Purrs);
    Expression last = Purrs::x(Purrs::Recurrence::n - 1).toGiNaC();
    Purrs::Expr rhs = Purrs::Expr::fromGiNaC(update.subs(target == last));
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {1, Purrs::Expr::fromGiNaC(update)} });

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            return false;
        }
        rec.exact_solution(exact);
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugPurrs("Purrs failed on x(n) = " << rhs << " with initial x(0)=" << target << " for update " << update);
        return false;
    }

    result = exact.toGiNaC();
    return true;
}


bool Recurrence::findCostRecurrence(Expression cost, Expression &result) {
    Timing::Scope timer(Timing::Purrs);
    cost = cost.subs(knownPreRecurrences); //replace variables by their recurrence equations

    //e.g. if cost = y, the result is x(n) = x(n-1) + y(n-1), with x(0) = 0
    Purrs::Expr rhs = Purrs::x(Purrs::Recurrence::n - 1) + Purrs::Expr::fromGiNaC(cost);
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {0, 0} }); //costs for no iterations are hopefully 0

        debugPurrs("COST REC: " << rhs);

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            return false;
        }
        rec.exact_solution(exact);
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugPurrs("Purrs failed on x(n) = " << rhs << " with initial x(0)=0 for cost" << cost);
        return false;
    }

    result = exact.toGiNaC();
    return true;
}


bool Recurrence::calcIteratedUpdate(const UpdateMap &oldUpdate, const Expression &meterfunc, UpdateMap &newUpdate) {
    assert(newUpdate.empty());

    UpdateMap update = oldUpdate; //might be changed by dependencyOrder, so copy here
    vector<VariableIndex> order = dependencyOrder(update);
    assert(order.size() == update.size());

    //in the given order try to solve the recurrence for every updated variable
    for (VariableIndex vi : order) {
        Expression res;
        ExprSymbol target = itrs.getGinacSymbol(vi);

        //use update rhs, but replace already processed variables with their recurrences
        Expression todo = update.at(vi).subs(knownPreRecurrences);
        if (!findUpdateRecurrence(todo,target,res)) {
            return false;
        }

        //remember this recurrence to replace vi in the updates depending on vi
        //note that updates need the value at n-1, e.g. x(n) = x(n-1) + vi(n-1) for the update x=x+vi
        knownPreRecurrences[target] = res.subs(ginacN == ginacN-1);

        //calcuate the final update using the loop's runtime
        newUpdate[vi] = res.subs(ginacN == meterfunc);
    }

    return true;
}


bool Recurrence::calcIteratedCost(const Expression &cost, const Expression &meterfunc, Expression &newCost) {
    //calculate the new cost sum
    Expression costRec;
    if (!findCostRecurrence(cost,costRec)) {
        return false;
    }
    newCost = costRec.subs(ginacN == meterfunc);
    return true;
}


bool Recurrence::calcIterated(const ITRSProblem &itrs, Transition &trans, const Expression &meterfunc) {
    Recurrence rec(itrs);

    UpdateMap newUpdate;
    if (!rec.calcIteratedUpdate(trans.update,meterfunc,newUpdate)) {
        debugPurrs("calcIterated: failed to calculate update recurrence");
        return false;
    }

    Expression newCost;
    if (!rec.calcIteratedCost(trans.cost,meterfunc,newCost)) {
        debugPurrs("calcIterated: failed to calculate cost recurrence");
        return false;
    }

    trans.update = std::move(newUpdate);
    trans.cost = newCost;
    trans.guard.insert(trans.guard.end(),rec.addGuard.begin(),rec.addGuard.end());
    return true;
}




