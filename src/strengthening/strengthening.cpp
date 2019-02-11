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

namespace {

    pair<GuardList, GuardList> splitInvariants(
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

    pair<GuardList, GuardList> splitMonotonicConstraints(
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

    pair<Expression, ExprSymbolSet> buildTemplate(
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

    GuardList instantiateTemplates(
            const vector<Expression> &templates,
            const ExprSymbolSet &params,
            const VariableManager &varMan,
            const z3::model &model,
            const Z3Context &context) {
        GuardList res;
        UpdateMap parameterInstantiation;
        for (const ExprSymbol &p: params) {
            optional<z3::expr> var = context.getVariable(p);
            if (var) {
                Expression pi = Z3Toolbox::getRealFromModel(model, var.get());
                parameterInstantiation.emplace(varMan.getVarIdx(p), pi);
            }
        }
        GiNaC::exmap subs = parameterInstantiation.toSubstitution(varMan);
        for (Expression t: templates) {
            t.applySubs(subs);
            ExprSymbolSet vars = t.getVariables();
            bool isInstantiated = true;
            for (const ExprSymbol &var: vars) {
                if (params.find(var) != params.end()) {
                    isInstantiated = false;
                    break;
                }
            }
            if (isInstantiated) {
                res.push_back(t);
            }
        }
        return res;
    }

    pair<z3::expr, vector<z3::expr>> constructSMTExpressions(
            const ExprSymbolSet &vars,
            Z3Context &context,
            const vector<Expression> &templates,
            const ExprSymbolSet &templateParams,
            const vector<GuardList> &preconditions,
            const GuardList &premise,
            const GuardList &conclusion) {
        z3::expr intExpr = FarkasLemma::apply(
                premise,
                conclusion,
                vars,
                templateParams,
                context,
                Z3Context::Integer);
        z3::expr_vector initiationValidVec(context);
        z3::expr_vector initiationSatVec(context);
        for (const GuardList &g: preconditions) {
            z3::expr_vector gVec(context);
            for (const Expression &e: g) {
                gVec.push_back(e.toZ3(context));
            }
            for (const Expression &t: templates) {
                vector<Expression> singleton(1, t);
                initiationValidVec.push_back(FarkasLemma::apply(
                        g,
                        singleton,
                        vars,
                        templateParams,
                        context,
                        Z3Context::Integer));
                initiationSatVec.push_back(t.toZ3(context) && mk_and(gVec));
            }
        }
        z3::expr initiationAllValid = mk_and(initiationValidVec);
        z3::expr initiationSomeValid = mk_or(initiationValidVec);
        z3::expr initiationAllSat = mk_and(initiationSatVec);
        z3::expr initiationSomeSat = mk_or(initiationSatVec);
        vector<z3::expr> res;
        res.push_back(initiationAllValid);
        res.push_back(initiationSomeValid && initiationAllSat);
        res.push_back(initiationSomeValid);
        res.push_back(initiationAllSat);
        res.push_back(initiationSomeSat);
        return pair<z3::expr, vector<z3::expr>>(intExpr, res);
    }

    bool addTemplateConstraints(
            const vector<GiNaC::exmap> &updates,
            const ExprSymbolSet &vars,
            const vector<Expression> &templates,
            GuardList &premise,
            GuardList &conclusion) {
        bool linearTemplate = false;
        for (const Expression &invTemplate: templates) {
                bool linear = true;
                GuardList updatedTemplates;
                for (const GiNaC::exmap &up: updates) {
                    Expression updated = invTemplate;
                    updated.applySubs(up);
                    if (Relation::isLinearInequality(updated, vars)) {
                        updatedTemplates.push_back(updated);
                    } else {
                        linear = false;
                        break;
                    }
                }
                if (linear) {
                    linearTemplate = true;
                    premise.push_back(invTemplate);
                    for (const Expression &t: updatedTemplates) {
                        conclusion.push_back(t);
                    }
                }
            }
        return linearTemplate;
    }

    option<pair<z3::expr, vector<z3::expr>>> buildHardAndSoftConstraints(
            const GuardList &todo,
            const GuardList &guard,
            const vector<GiNaC::exmap> &updates,
            const ExprSymbolSet &vars,
            VariableManager &varMan,
            Z3Context &context,
            const vector<Expression> &templates,
            const ExprSymbolSet &templateParams,
            const vector<GuardList> &preconditions) {
        GuardList premise;
        GuardList conclusion;
        for (const Expression &e: guard) {
            premise.push_back(e);
        }
        for (const Expression &g: todo) {
            for (const GiNaC::exmap &up: updates) {
                Expression updated = g;
                updated.applySubs(up);
                if (Relation::isLinearInequality(updated, vars)) {
                    conclusion.push_back(updated);
                }
            }
        }
        if (!conclusion.empty()) {
            if (addTemplateConstraints(updates, vars, templates, premise, conclusion)) {
                return constructSMTExpressions(
                        vars,
                        context,
                        templates,
                        templateParams,
                        preconditions,
                        premise,
                        conclusion);
            }
        }
        return option<pair<z3::expr, vector<z3::expr>>>();
    }

    ExprSymbolSet findRelevantVariables(const VarMan &varMan, const Expression &c, const Rule &r) {
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

    GuardList findRelevantConstraints(const GuardList &guard, const ExprSymbolSet &vars) {
        GuardList relevantConstraints;
        for (const Expression &e: guard) {
            for (const ExprSymbol &var: vars) {
                if (e.getVariables().count(var) > 0) {
                    relevantConstraints.push_back(e);
                    break;
                }
            }
        }
        return relevantConstraints;
    }

    GuardList tryToForceInvariance(
            const Rule &r,
            const GuardList &todo,
            const vector<GuardList> &preconditions,
            VariableManager &varMan) {
        Z3Context context;
        Z3Solver solver(context);
        GuardList succeeded;
        GuardList failed;
        vector<UpdateMap> multiUpdate;
        vector<GiNaC::exmap> updates;
        for (const RuleRhs &u: r.getRhss()) {
            multiUpdate.push_back(u.getUpdate());
            updates.push_back(u.getUpdate().toSubstitution(varMan));
        }
        ExprSymbolSet allRelevantVariables;
        vector<Expression> templates;
        ExprSymbolSet templateParams;
        for (const Expression &g: todo) {
            ExprSymbolSet varSymbols = findRelevantVariables(varMan, g, r);
            allRelevantVariables.insert(varSymbols.begin(), varSymbols.end());
            pair<Expression, ExprSymbolSet> p = buildTemplate(varSymbols, varMan);
            templates.push_back(p.first);
            templateParams.insert(p.second.begin(), p.second.end());
        }
        GuardList relevantConstraints = findRelevantConstraints(r.getGuard(), allRelevantVariables);
        optional<pair<z3::expr, vector<z3::expr>>> smtExpressions = buildHardAndSoftConstraints(
                todo,
                relevantConstraints,
                updates,
                allRelevantVariables,
                varMan,
                context,
                templates,
                templateParams,
                preconditions);
        if (smtExpressions) {
            z3::expr hard = smtExpressions.get().first;
            vector<z3::expr> soft = smtExpressions.get().second;
            solver.add(hard);
            z3::check_result res = solver.check();
            debugInvariants("hard:" << res);
            if (res == z3::check_result::sat) {
                for (const z3::expr &s: soft) {
                    solver.push();
                    solver.add(s);
                    res = solver.check();
                    if (res == z3::check_result::sat) {
                        GuardList newInvariants = instantiateTemplates(
                                templates,
                                templateParams,
                                varMan,
                                solver.get_model(),
                                context);
                        for (const Expression &e: newInvariants) {
                            debugInvariants("new invariant " << e);
                        }
                        return newInvariants;
                    } else {
                        solver.pop();
                    }
                }
            }
        }
        return GuardList();
    }

    vector<GuardList> buildPreconditions(
            const vector<Rule> &predecessors,
            const Rule &r,
            const VariableManager &varMan) {
        vector<GuardList> res;
        for (const Rule &pred: predecessors) {
            if (pred.getGuard() == r.getGuard()) {
                continue;
            }
            for (const RuleRhs &rhs: pred.getRhss()) {
                if (rhs.getLoc() == r.getLhsLoc()) {
                    GiNaC::exmap up = rhs.getUpdate().toSubstitution(varMan);
                    GuardList pre;
                    for (Expression g: r.getGuard()) {
                        g.applySubs(up);
                        pre.push_back(g);
                    }
                    for (const auto &p: rhs.getUpdate()) {
                        ExprSymbol var = varMan.getVarSymbol(p.first);
                        Expression varUpdate = p.second;
                        Expression updatedVarUpdate = varUpdate;
                        updatedVarUpdate.applySubs(up);
                        if (varUpdate == updatedVarUpdate) {
                            pre.push_back(var == varUpdate);
                        }
                    }
                    res.push_back(pre);
                }
            }
        }
        return res;
    }

}

optional<Rule> Strengthening::apply(
        const vector<Rule> &predecessors,
        const Rule &r, VariableManager &varMan) {
    if (!r.isSimpleLoop()) {
        return optional<Rule>();
    }
    pair<GuardList, GuardList> p = splitInvariants(r, varMan);
    GuardList invariants = p.first;
    GuardList nonInvariants = p.second;
    p = splitMonotonicConstraints(r, invariants, nonInvariants, varMan);
    GuardList monotonic = p.first;
    GuardList nonMonotonic = p.second;
    if (!nonMonotonic.empty()) {
        vector<GuardList> preconditions = buildPreconditions(predecessors, r, varMan);
        GuardList todo;
        for (const Expression &e: nonMonotonic) {
            if (Relation::isLinearEquality(e)) {
                todo.emplace_back(e.lhs() <= e.rhs());
                todo.emplace_back(e.rhs() <= e.lhs());
            } else if (Relation::isLinearInequality(e)) {
                todo.push_back(e);
            }
        }
        GuardList newInvariants = tryToForceInvariance(r, todo, preconditions, varMan);
        if (!newInvariants.empty()) {
            GuardList newGuard(r.getGuard());
            for (const Expression &g: newInvariants) {
                newGuard.push_back(g);
            }
            return Rule(RuleLhs(r.getLhsLoc(), newGuard, r.getCost()), r.getRhss());
        }
    }
    return optional<Rule>();
}
