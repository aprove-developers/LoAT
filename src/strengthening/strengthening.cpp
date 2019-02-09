//
// Created by ffrohn on 2/8/19.
//

#include <expr/relation.h>
#include <accelerate/meter/farkas.h>
#include "strengthening.h"
#include "z3/z3solver.h"
#include "z3/z3toolbox.h"
#include "accelerate/meter/metertools.h"

using boost::optional;
using namespace std;

static pair<GuardList, GuardList> splitInvariants(
        const Rule &r,
        const VariableManager &varMan) {
    Z3Context context;
    Z3Solver solver(context);
    for (const Expression &g: r.getGuard()) {
        solver.add(g.toZ3(context));
    }
    vector<GiNaC::exmap> updates;
    for (const RuleRhs &rhs: r.getRhss()) {
        updates.push_back(rhs.getUpdate().toSubstitution(varMan));
    }
    GuardList invariants;
    GuardList nonInvariants;
    for (const Expression &g: r.getGuard()) {
        bool isInvariant = true;
        for (const GiNaC::exmap &up: updates) {
            Expression conclusionExp = g;
            conclusionExp.applySubs(up);
            z3::expr conclusion = conclusionExp.toZ3(context);
            solver.push();
            solver.add(!conclusion);
            z3::check_result res = solver.check();
            solver.pop();
            if (res != z3::check_result::unsat) {
                isInvariant = false;
                break;
            }
        }
        if (isInvariant) {
            invariants.push_back(g);
        } else {
            nonInvariants.push_back(g);
        }
    }
    return pair<GuardList, GuardList>(invariants, nonInvariants);
}

static pair<GuardList, GuardList> splitMonotonicConstraints(
        const Rule &r,
        const GuardList &invariants,
        const GuardList &nonInvariants,
        const VariableManager &varMan) {
    Z3Context context;
    Z3Solver solver(context);
    for (const Expression &g: invariants) {
        solver.add(g.toZ3(context));
    }
    GuardList monotonic(nonInvariants);
    GuardList nonMonotonic;
    for (const RuleRhs &rhs: r.getRhss()) {
        solver.push();
        GiNaC::exmap up = rhs.getUpdate().toSubstitution(varMan);
        for (Expression g: r.getGuard()) {
            g.applySubs(up);
            solver.add(g.toZ3(context));
        }
        auto g = monotonic.begin();
        while (g != monotonic.end()) {
            solver.push();
            solver.add(!(*g).toZ3(context));
            z3::check_result res = solver.check();
            solver.pop();
            if (res == z3::check_result::unsat) {
                g++;
            } else {
                nonMonotonic.push_back(*g);
                g = monotonic.erase(g);
            }
        }
        solver.pop();
    }
    return pair<GuardList, GuardList>(monotonic, nonMonotonic);
}

static pair<Expression, ExprSymbolSet> buildTemplate(
        const ExprSymbolSet &vars,
        VariableManager &varMan) {
    ExprSymbolSet params;
    ExprSymbol c0 = varMan.getVarSymbol(varMan.addFreshVariable("c0"));
    params.insert(c0);
    Expression lhs = Expression(0);
    for (const ExprSymbol &x: vars) {
        ExprSymbol param = varMan.getVarSymbol(varMan.addFreshVariable("c"));
        params.insert(param);
        lhs = lhs + (x * param);
    }
    Expression res = lhs <= c0;
    return pair<Expression, ExprSymbolSet>(res, params);
}

static vector<Expression> instantiateTemplates(
        const vector<Expression> &templates,
        const ExprSymbolSet &params,
        const VariableManager &varMan,
        const z3::model &model,
        const Z3Context &context) {
    vector<Expression> res;
    UpdateMap parameterInstantiation;
    for (const ExprSymbol &p: params) {
        Expression pi = Z3Toolbox::getRealFromModel(model, context.getVariable(p).get());
        parameterInstantiation.emplace(varMan.getVarIdx(p), pi);
    }
    GiNaC::exmap subs = parameterInstantiation.toSubstitution(varMan);
    for (Expression t: templates) {
        t.applySubs(subs);
        res.push_back(t);
    }
    return res;
}

static optional<vector<Expression>> tryToForceInvariance(
        const Expression &g,
        const GuardList &guard,
        const vector<GiNaC::exmap> &updates,
        const ExprSymbolSet &vars,
        VariableManager &varMan) {
    Z3Context context;
    Z3Solver solver(context);
    GuardList premise;
    GuardList conclusion;
    for (const Expression &e: guard) {
        premise.push_back(e);
    }
    for (const GiNaC::exmap &up: updates) {
        Expression updated = g;
        updated.applySubs(up);
        if (!Relation::isLinearInequality(updated, vars)) {
            return optional<vector<Expression>>{};
        }
        conclusion.push_back(updated);
    }
    vector<Expression> templates;
    ExprSymbolSet templateParams;
    for (int i = 0; i < Config::Invariants::NumTemplates; i++) {
        pair<Expression, ExprSymbolSet> p = buildTemplate(vars, varMan);
        Expression invTemplate = p.first;
        for (const ExprSymbol &param: p.second) {
            templateParams.insert(param);
        }
        templates.push_back(invTemplate);
        premise.push_back(invTemplate);
        for (const GiNaC::exmap &up: updates) {
            Expression updated = invTemplate;
            updated.applySubs(up);
            if (!Relation::isLinearInequality(updated, vars)) {
                return optional<vector<Expression>>{};
            }
            conclusion.push_back(updated);
        }
    }
    solver.add(FarkasLemma::apply(premise, conclusion, vars, templateParams, context, Z3Context::Integer));
    z3::check_result res = solver.check();
    if (res != z3::check_result::sat) {
        solver.reset();
        solver.add(FarkasLemma::apply(premise, conclusion, vars, templateParams, context, Z3Context::Real));
        res = solver.check();
    }
    if (res == z3::check_result::sat) {
        vector<Expression> newInvariants = instantiateTemplates(
                templates,
                templateParams,
                varMan,
                solver.get_model(),
                context);
        return newInvariants;
    } else {
        return optional<vector<Expression>>{};
    }
}

static ExprSymbolSet findRelevantVariables(const VarMan &varMan, const Expression &c, const Rule &r) {
    set<VariableIdx> res;
    // Add all variables appearing in the guard
    ExprSymbolSet guardVariables = c.getVariables();
    for (const ExprSymbol &sym : guardVariables) {
        res.insert(varMan.getVarIdx(sym));
    }
    // Compute the closure of res under all updates and the guard
    set<VariableIdx> todo = res;
    while (!todo.empty()) {
        ExprSymbolSet next;

        for (VariableIdx var : todo) {
            for (const RuleRhs &rhs: r.getRhss()) {
                UpdateMap update = rhs.getUpdate();
                auto it = update.find(var);
                if (it != update.end()) {
                    ExprSymbolSet rhsVars = it->second.getVariables();
                    next.insert(rhsVars.begin(), rhsVars.end());
                }
            }
            for (const Expression &g: r.getGuard()) {
                ExprSymbolSet gVars = g.getVariables();
                if (gVars.find(varMan.getVarSymbol(var)) != gVars.end()) {
                    next.insert(gVars.begin(), gVars.end());
                }
            }
        }
        todo.clear();
        for (const ExprSymbol &sym : next) {
            VariableIdx var = varMan.getVarIdx(sym);
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

static GuardList findRelevantConstraints(const GuardList &guard, ExprSymbolSet vars) {
    GuardList relevantConstraints;
    for (const Expression &e: guard) {
        for (const ExprSymbol &var: vars) {
            if (e.getVariables().count(var) > 0) {
                relevantConstraints.push_back(e);
            }
        }
    }
    return relevantConstraints;
}

static pair<GuardList, GuardList> tryToForceInvariance(
        const Rule &r,
        const GuardList &todo,
        VariableManager &varMan) {
    GuardList succeeded;
    GuardList failed;
    vector<UpdateMap> multiUpdate;
    vector<GiNaC::exmap> updates;
    for (const RuleRhs &u: r.getRhss()) {
        multiUpdate.push_back(u.getUpdate());
        updates.push_back(u.getUpdate().toSubstitution(varMan));
    }
    for (const Expression &g: todo) {
        ExprSymbolSet varSymbols = findRelevantVariables(varMan, g, r);
        GuardList relevantConstraints = findRelevantConstraints(r.getGuard(), varSymbols);
        optional<vector<Expression>> newInvariants = tryToForceInvariance(
                g,
                relevantConstraints,
                updates,
                varSymbols,
                varMan);
        if (newInvariants) {
            succeeded.push_back(g);
            for (const Expression &c: newInvariants.get()) {
                succeeded.push_back(c);
            }
        } else {
            failed.push_back(g);
        }
    }
    return pair<GuardList, GuardList>(succeeded, failed);
}

optional<Rule> Strengthening::apply(const Rule &r, VariableManager &varMan) {
    if (!r.isSimpleLoop()) {
        return optional<Rule>{};
    }
    pair<GuardList, GuardList> p = splitInvariants(r, varMan);
    GuardList invariants = p.first;
    GuardList nonInvariants = p.second;
    p = splitMonotonicConstraints(r, invariants, nonInvariants, varMan);
    GuardList monotonic = p.first;
    GuardList nonMonotonic = p.second;
    p = tryToForceInvariance(r, nonMonotonic, varMan);
    GuardList newInvariants = p.first;
    GuardList failed = p.second;
    if (newInvariants.empty()) {
        return optional<Rule>{};
    } else {
        GuardList newGuard;
        for (const Expression &g: invariants) {
            newGuard.push_back(g);
        }
        for (const Expression &g: monotonic) {
            newGuard.push_back(g);
        }
        for (const Expression &g: newInvariants) {
            debugInvariants("new invariant: " << g);
            newGuard.push_back(g);
        }
        for (const Expression &g: failed) {
            newGuard.push_back(g);
        }
        return Rule(RuleLhs(r.getLhsLoc(), newGuard, r.getCost()), r.getRhss());
    }
}
