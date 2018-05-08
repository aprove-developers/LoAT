#include "backwardacceleration.h"

#include "debug.h"
#include "z3toolbox.h"

#include <purrs.hh>

namespace Purrs = Parma_Recurrence_Relation_Solver;
using namespace std;

BackwardAcceleration::BackwardAcceleration(ITRSProblem &i, Transition t): trans(t), itrs(i), ginac_n(Purrs::Expr(Purrs::Recurrence::n).toGiNaC()) {}

// TODO copied from farkas.cpp -- refactor
vector<Expression> BackwardAcceleration::reduceGuard(Z3VariableContext &c) {
    vector<Expression> reducedGuard;

    //create Z3 solver here to use push/pop for efficiency
    Z3Solver sol(c);
    z3::params params(c);
    params.set(":timeout", Z3_CHECK_TIMEOUT);
    sol.set(params);
    for (const Expression &ex : trans.guard) sol.add(ex.toZ3(c));

    for (Expression ex : trans.guard) {
        bool add = false;
        bool add_always = false;
        GiNaC::exmap updateSubs;
        auto varnames = ex.getVariableNames();
        for (string varname : varnames) {
            VariableIndex vi = itrs.getVarindex(varname);
            //keep constraint if it contains a free variable
            if (itrs.isFreeVar(vi)) {
                add_always = true;
            }
            //keep constraint if it contains an updated variable
            auto upIt = trans.update.find(vi);
            if (upIt != trans.update.end()) {
                add = true;
                updateSubs[itrs.getGinacSymbol(upIt->first)] = upIt->second;
            }
        }
        //add if ex contains free var OR updated variable and is not trivially true for the update
        if (add_always) {
            reducedGuard.push_back(ex);
        } else if (add) {
            sol.push();
            sol.add(!Expression::ginacToZ3(ex.subs(updateSubs),c));
            bool tautology = (sol.check() == z3::unsat);
            if (!tautology) {
                reducedGuard.push_back(ex);
                sol.pop();
            }
        }
    }
    return reducedGuard;
}

// TODO copied from recurrence.cpp -- refactor
bool findUpdateRecurrence(Expression update, ExprSymbol target, Expression &result) {
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

// TODO overlaps with the corresponding method from recurrence.cpp -- refactor
boost::optional<vector<VariableIndex>> BackwardAcceleration::dependencyOrder(UpdateMap &update) {
    vector<VariableIndex> ordering;
    set<VariableIndex> orderedVars;

    bool changed = true;
    while (changed && ordering.size() < update.size()) {
        changed = false;
        for (auto up : update) {
            if (orderedVars.count(up.first) > 0) continue;

            bool ready = true;
            //check if all variables on update rhs are already processed
            for (const string &varname : up.second.getVariableNames()) {
                VariableIndex vi = itrs.getVarindex(varname);
                if (vi != up.first && update.count(vi) > 0 && orderedVars.count(vi) == 0) {
                    ready = false;
                    break;
                }
            }

            if (ready) {
                changed = changed | orderedVars.insert(up.first).second;
                ordering.push_back(up.first);
            }
        }

    }
    if (ordering.size() == update.size()) {
        debugBackwardAcceleration("successfully computed dependency order");
        return boost::optional<vector<VariableIndex>>(ordering);
    } else {
        debugBackwardAcceleration("failed to compute dependency order");
        return boost::optional<vector<VariableIndex>>();
    }
}

bool BackwardAcceleration::shouldAccelerate() {
    if (trans.cost.is_polynomial(itrs.getGinacVarList())) {
        return true;
    } else {
        debugBackwardAcceleration("won't try to accelerate transition with costs " << trans.cost);
        return false;
    }
}

boost::optional<GiNaC::exmap> BackwardAcceleration::computeInverseUpdate(vector<VariableIndex> order) {
    set<VariableIndex> relevant_vars;
    for (Expression e: trans.guard) {
        for (string s: e.getVariableNames()) {
            relevant_vars.insert(itrs.getVarindex(s));
        }
    }
    bool changed;
    // we also need to know the inverse update for every variable that occurs in the update of a relevant variable
    do {
        changed = false;
        for (VariableIndex vi: relevant_vars) {
            if (trans.update.find(vi) == trans.update.end()) {
                continue;
            }
            for (string s: trans.update[vi].getVariableNames()) {
                changed = changed || relevant_vars.insert(itrs.getVarindex(s)).second;
            }
        }
    } while (changed);
    GiNaC::exmap inverse_update;
    for (VariableIndex vi: order) {
        if (relevant_vars.find(vi) == relevant_vars.end() || trans.update.find(vi) == trans.update.end()) {
            continue;
        }
        Expression x = itrs.getGinacSymbol(vi);
        GiNaC::lst xList;
        xList.append(x);
        Expression up = trans.update.at(vi);
        if (!up.isLinear(xList)) {
            debugBackwardAcceleration("update " << up << " is not linear");
            return boost::optional<GiNaC::exmap>();
        }
        Expression lincoeff = up.coeff(x, 1);
        Expression in_up;
        if (lincoeff.is_zero()) {
            in_up = up;
        } else {
            in_up = (x / lincoeff) - (up - lincoeff * x) / lincoeff;
        }
        in_up = in_up.subs(inverse_update);
        inverse_update.emplace(x, in_up);
    }
    debugBackwardAcceleration("successfully computed inverse update " << inverse_update);
    return boost::optional<GiNaC::exmap>(inverse_update);
}

bool BackwardAcceleration::checkGuardImplication(GiNaC::exmap inverse_update) {
    Z3VariableContext c;
    vector<Expression> reduced_guard = reduceGuard(c);
    z3::expr rhs = c.bool_val(true);
    for (Expression e: reduced_guard) {
        rhs = rhs && Expression::ginacToZ3(e.subs(inverse_update), c);
    }
    z3::expr lhs = c.bool_val(true);
    for (Expression e: trans.guard) {
        lhs = lhs && Expression::ginacToZ3(e, c);
    }
    Z3Solver solver(c);
    z3::params params(c);
    params.set(":timeout", Z3_CHECK_TIMEOUT);
    solver.set(params);
    solver.add(!rhs && lhs);
    if (solver.check() == z3::unsat) {
        debugBackwardAcceleration("successfully checked guard implication " << lhs << " ==> " << rhs);
        return true;
    } else {
        debugBackwardAcceleration("failed to check guard implication");
        return false;
    }
}

boost::optional<GiNaC::exmap> BackwardAcceleration::computeIteratedUpdate(UpdateMap update, vector<VariableIndex> order) {
    GiNaC::exmap iterated_update;
    for (VariableIndex vi: order) {
        Expression res;
        ExprSymbol target = itrs.getGinacSymbol(vi);
        Expression todo = update.at(vi).subs(iterated_update);
        if (!findUpdateRecurrence(todo, target, res)) {
            debugBackwardAcceleration("failed to compute iterated update for " << target);
            return boost::optional<GiNaC::exmap>();
        }
        debugBackwardAcceleration("successfully computed iterated update " << res << " for " << target);
        iterated_update[target] = res;
    }
    return boost::optional<GiNaC::exmap>(iterated_update);
}

// TODO mostly copied from Recurrence::findCostRecurrence -- refactor
boost::optional<Expression> BackwardAcceleration::computeIteratedCosts(GiNaC::exmap iterated_update) {
    debugBackwardAcceleration("computing iterated costs");
    Timing::Scope timer(Timing::Purrs);
    Expression cost = trans.cost.subs(iterated_update); //replace variables by their recurrence equations

    //e.g. if cost = y, the result is x(n) = x(n-1) + y(n-1), with x(0) = 0
    Purrs::Expr rhs = Purrs::x(Purrs::Recurrence::n - 1) + Purrs::Expr::fromGiNaC(cost);
    Purrs::Expr sol;

    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {0, 0} }); //costs for no iterations are hopefully 0

        debugPurrs("COST REC: " << rhs);

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            res = rec.compute_lower_bound();
            if (res != Purrs::Recurrence::SUCCESS) {
                debugBackwardAcceleration("Purrs failed on x(n) = " << rhs << " with initial x(0)=0 for cost" << cost);
                return boost::optional<Expression>();
            } else {
                rec.lower_bound(sol);
            }
        } else {
            rec.exact_solution(sol);
        }
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugBackwardAcceleration("Purrs failed on x(n) = " << rhs << " with initial x(0)=0 for cost" << cost);
        return boost::optional<Expression>();
    }

    debugBackwardAcceleration("successfully computed iterated costs");
    return boost::optional<Expression>(sol.toGiNaC());
}

Transition BackwardAcceleration::buildNewTransition(GiNaC::exmap iterated_update, Expression iterated_costs) {
    Transition new_transition;
    // use a fresh variable to represent the number of iterations to avoid clashes
    Expression n = itrs.getGinacSymbol(itrs.addFreshVariable("n", true));
    GiNaC::exmap var_map;
    new_transition.guard = trans.guard;
    // the number of iterations needs to be positive
    new_transition.guard.push_back(n > 0);
    for (pair<VariableIndex, Expression> p: trans.update) {
        VariableIndex vi = p.first;
        new_transition.update.emplace(vi, iterated_update.at(itrs.getGinacSymbol(vi)).subs(ginac_n == n));
    }
    for (Expression e: trans.guard) {
        new_transition.guard.push_back(e.subs(iterated_update).subs(ginac_n == n-1));
    }
    new_transition.cost = iterated_costs.subs(ginac_n == n);
    debugBackwardAcceleration("backward-accelerating " << trans << " yielded " << new_transition);
    return new_transition;
}

boost::optional<Transition> BackwardAcceleration::accelerate() {
    if (shouldAccelerate()) {
        boost::optional<vector<VariableIndex>> order = dependencyOrder(trans.update);
        if (order) {
            boost::optional<GiNaC::exmap> inverse_update = computeInverseUpdate(*order);
            if (inverse_update) {
                if (checkGuardImplication(*inverse_update)) {
                    boost::optional<GiNaC::exmap> iterated_update = computeIteratedUpdate(trans.update, *order);
                    if (iterated_update) {
                        boost::optional<Expression> iterated_costs = computeIteratedCosts(*iterated_update);
                        if (iterated_costs) {
                            Transition new_transition = buildNewTransition(*iterated_update, *iterated_costs);
                            return boost::optional<Transition>(new_transition);
                        }
                    }
                }
            }
        }
    }
    return boost::optional<Transition>();
}
