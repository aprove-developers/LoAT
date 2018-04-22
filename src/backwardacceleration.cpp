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

bool BackwardAcceleration::costFunctionSupported() {
    if (trans.cost.is_polynomial(itrs.getGinacVarList())) {
        return true;
    } else {
        debugBackwardAcceleration("cost function " << trans.cost << " not supported");
        return false;
    }
}

boost::optional<pair<GiNaC::exmap, UpdateMap>> BackwardAcceleration::computeInverseUpdate() {
    GiNaC::exmap inverse_ginac_update;
    UpdateMap inverse_update;
    for (pair<VariableIndex, Expression> p: trans.update) {
        Expression x = itrs.getGinacSymbol(p.first);
        GiNaC::lst xList;
        xList.append(x);
        Expression up = p.second;
        if (!up.isLinear(xList)) {
            debugBackwardAcceleration("update " << up << " is not linear");
            return boost::optional<pair<GiNaC::exmap, UpdateMap>>();
        }
        Expression lincoeff = up.coeff(x, 1);
        Expression in_up;
        if (lincoeff.is_zero()) {
            in_up = up;
        } else {
            in_up = (x / lincoeff) - (up - lincoeff * x) / lincoeff;
        }
        inverse_ginac_update.emplace(x, in_up);
        inverse_update.emplace(p.first, in_up);
    }
    debugBackwardAcceleration("successfully computed inverse update " << inverse_ginac_update);
    return boost::optional<pair<GiNaC::exmap, UpdateMap>>(pair<GiNaC::exmap, UpdateMap>(inverse_ginac_update, inverse_update));
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

boost::optional<GiNaC::exmap> BackwardAcceleration::computeIteratedInverseUpdate(GiNaC::exmap inverse_update, vector<VariableIndex> order) {
    GiNaC::exmap iterated_inverse_update;
    for (VariableIndex vi: order) {
        Expression res;
        ExprSymbol target = itrs.getGinacSymbol(vi);
        Expression todo = inverse_update.at(target).subs(iterated_inverse_update);
        if (!findUpdateRecurrence(todo, target, res)) {
            debugBackwardAcceleration("failed to compute iterated inverse update for " << target);
            return boost::optional<GiNaC::exmap>();
        }
        debugBackwardAcceleration("successfully computed iterated inverse update " << res << " for " << target);
        iterated_inverse_update[target] = res;
    }
    return boost::optional<GiNaC::exmap>(iterated_inverse_update);
}

// TODO very similar to Recurrence::findCostRecurrence -- refactor
boost::optional<Expression> BackwardAcceleration::computeIteratedCosts(GiNaC::exmap iterated_inverse_update) {
    Expression rhs_costs = trans.cost.subs(iterated_inverse_update).subs(ginac_n == ginac_n - 1);
    Purrs::Expr rhs = Purrs::x(Purrs::Recurrence::n - 1) + Purrs::Expr::fromGiNaC(rhs_costs);
    Purrs::Expr exact;
    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {1, Purrs::Expr::fromGiNaC(trans.cost)} });
        debugBackwardAcceleration("recurrence equations for iterated costs: x(n) = " << rhs << ", x(1) = " << trans.cost);
        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            res = rec.compute_lower_bound();
            if (res != Purrs::Recurrence::SUCCESS) {
                debugBackwardAcceleration("failed to compute iterated costs");
                return boost::optional<Expression>();
            }
        }
        rec.exact_solution(exact);
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugBackwardAcceleration("failed to compute iterated costs");
        return boost::optional<Expression>();
    }
    Expression res = exact.toGiNaC();
    debugBackwardAcceleration("successfully computed iterated costs " << res << " for " << trans.cost);
    return boost::optional<Expression>(res);
}

Transition BackwardAcceleration::buildNewTransition(GiNaC::exmap inverse_update, GiNaC::exmap iterated_inverse_update, Expression iterated_costs) {
    Transition new_transition;
    // use a fresh variable to represent the number of iterations to avoid clashes
    Expression n = itrs.getGinacSymbol(itrs.addFreshVariable("n", true));
    GiNaC::exmap var_map;
    new_transition.guard = trans.guard;
    // the number of iterations needs to be positive
    new_transition.guard.push_back(n > 0);
    for (pair<VariableIndex, Expression> p: trans.update) {
        VariableIndex vi = p.first;
        Expression old_var = itrs.getGinacSymbol(vi);
        // create a fresh variable which represents the value of the currently processed variable after applying the accelerated transition
        VariableIndex fresh = itrs.addFreshVariable(itrs.getVarname(vi), true);
        Expression fresh_var = itrs.getGinacSymbol(fresh);
        new_transition.update.emplace(vi, fresh_var);
        // we computed the iterated _inverse_ update, so replace old_var (representing the value before the accelerated transition)
        // with new_var (representing the value after the accelerated transition)
        GiNaC::exmap sigma;
        sigma.emplace(old_var, fresh_var);
        var_map.emplace(old_var, fresh_var);
        sigma.emplace(ginac_n, n);
        Expression it_up = iterated_inverse_update.at(old_var);
        // now the value before the transition (old_var) needs to result from the value after the transition by applying the iterated inverse update
        new_transition.guard.push_back(old_var == it_up.subs(sigma));
        GiNaC::lst old_var_list;
        old_var_list.append(old_var);
        if (it_up.isLinear(old_var_list)) {
            Expression coeff = it_up.coeff(old_var);
            if (!coeff.is_zero()) {
                Expression up = ((it_up - coeff * old_var - fresh_var) / coeff).subs(ginac_n == n);
                if (mapsToInt(up)) {
                    new_transition.update[vi] = up;
                }
            }
        }
        Expression rhs = trans.update.at(vi).subs(inverse_update).subs(sigma);
        new_transition.guard.push_back(fresh_var == rhs);
    }
    new_transition.cost = iterated_costs.subs(ginac_n == n);
    // make sure that the transition can be applied in the last step
    for (Expression e: trans.guard) {
        Expression final_constraint = e.subs(inverse_update);
        if (e != final_constraint) {
            new_transition.guard.push_back(final_constraint.subs(var_map));
        }
    }
    debugBackwardAcceleration("backward-accelerating " << trans << " yielded " << new_transition);
    return new_transition;
}

bool BackwardAcceleration::mapsToInt(Expression e) {
    debugBackwardAcceleration("checking if " << e << " maps to int");
    auto vars = itrs.getGinacVarList();
    if (!e.is_polynomial(vars)) {
        return false;
    }
    vector<int> degrees;
    vector<int> subs;
    for (Expression x: vars) {
        GiNaC::exmap x_subs;
        for (Expression y: vars) {
            if (x != y) {
                x_subs.emplace(y, 1);
            }
        }
        degrees.push_back(e.subs(x_subs).degree(x));
        subs.push_back(0);
    }
    while (true) {
        bool found_next = false;
        for (int i = 0; i < degrees.size(); i++) {
            if (subs[i] == degrees[i]) {
                subs[i] = 0;
            } else {
                subs[i] = subs[i] + 1;
                found_next = true;
                break;
            }
        }
        if (!found_next) {
            debugBackwardAcceleration("it does!");
            return true;
        }
        GiNaC::exmap g_subs;
        for (int i = 0; i < degrees.size(); i++) {
            g_subs.emplace(vars[i], subs[i]);
        }
        Expression res = e.subs(g_subs);
        if (!e.subs(g_subs).info(GiNaC::info_flags::integer)) {
            debugBackwardAcceleration("it does not for " << g_subs << " where it yields " << res);
            return false;
        }
    }
}

boost::optional<Transition> BackwardAcceleration::accelerate() {
    if (costFunctionSupported()) {
        boost::optional<pair<GiNaC::exmap, UpdateMap>> inverse_update = computeInverseUpdate();
        if (inverse_update.is_initialized()) {
            GiNaC::exmap i_up = inverse_update.get().first;
            if (checkGuardImplication(i_up)) {
                boost::optional<vector<VariableIndex>> order = dependencyOrder(inverse_update.get().second);
                if (order.is_initialized()) {
                    boost::optional<GiNaC::exmap> iterated_inverse_update = computeIteratedInverseUpdate(i_up, order.get());
                    if (iterated_inverse_update.is_initialized()) {
                        boost::optional<Expression> iterated_costs = computeIteratedCosts(iterated_inverse_update.get());
                        if (iterated_costs.is_initialized()) {
                            Transition new_transition = buildNewTransition(i_up, iterated_inverse_update.get(), iterated_costs.get());
                            return boost::optional<Transition>(new_transition);
                        }
                    }
                }
            }
        }
    }
    return boost::optional<Transition>();
}
