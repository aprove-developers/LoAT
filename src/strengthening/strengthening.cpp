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

    const pair<GuardList, GuardList> splitInvariants(
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
                const z3::expr &conclusion = conclusionExp.toZ3(context);
                solver.push();
                solver.add(!conclusion);
                const z3::check_result &res = solver.check();
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
        return {invariants, nonInvariants};
    }

    const pair<GuardList, GuardList> splitMonotonicConstraints(
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
            const GiNaC::exmap &up = rhs.getUpdate().toSubstitution(varMan);
            for (Expression g: r.getGuard()) {
                g.applySubs(up);
                solver.add(g.toZ3(context));
            }
            auto g = monotonic.begin();
            while (g != monotonic.end()) {
                solver.push();
                solver.add(!(*g).toZ3(context));
                const z3::check_result &res = solver.check();
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
        return {monotonic, nonMonotonic};
    }

    const pair<Expression, ExprSymbolSet> buildTemplate(
            const ExprSymbolSet &vars,
            VariableManager &varMan) {
        ExprSymbolSet params;
        const ExprSymbol &c0 = varMan.getVarSymbol(varMan.addFreshVariable("c0"));
        params.insert(c0);
        Expression res = c0;
        for (const ExprSymbol &x: vars) {
            const ExprSymbol &param = varMan.getVarSymbol(varMan.addFreshVariable("c"));
            params.insert(param);
            res = res + (x * param);
        }
        return {res, params};
    }

    const GuardList instantiateTemplates(
            const vector<Expression> &templates,
            const ExprSymbolSet &params,
            const VariableManager &varMan,
            const z3::model &model,
            const Z3Context &context) {
        GuardList res;
        UpdateMap parameterInstantiation;
        for (const ExprSymbol &p: params) {
            const optional<z3::expr> &var = context.getVariable(p);
            if (var) {
                const Expression &pi = Z3Toolbox::getRealFromModel(model, var.get());
                parameterInstantiation.emplace(varMan.getVarIdx(p), pi);
            }
        }
        const GiNaC::exmap &subs = parameterInstantiation.toSubstitution(varMan);
        for (Expression t: templates) {
            t.applySubs(subs);
            const ExprSymbolSet &vars = t.getVariables();
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

    const pair<vector<z3::expr>, vector<z3::expr>> constructSMTExpressions(
            const ExprSymbolSet &vars,
            Z3Context &context,
            const vector<Expression> &templates,
            const ExprSymbolSet &templateParams,
            const vector<GuardList> &preconditions,
            const GuardList &premise,
            const GuardList &conclusion,
            VariableManager &varMan) {
        const z3::expr &validImplication = FarkasLemma::apply(
                premise,
                conclusion,
                vars,
                templateParams,
                context,
                Z3Context::Integer);
        vector<z3::expr> soft;
        z3::expr_vector someSat(context);
        for (const GuardList &g: preconditions) {
            z3::expr_vector gVec(context);
            for (const Expression &e: g) {
                gVec.push_back(e.toZ3(context));
            }
            for (const Expression &t: templates) {
                soft.push_back(FarkasLemma::apply(
                        g,
                        {t},
                        vars,
                        templateParams,
                        context,
                        Z3Context::Integer));
            }
            ExprSymbolSet allVars(vars);
            g.collectVariables(allVars);
            for (const Expression &e: premise) {
                e.collectVariables(allVars);
            }
            GiNaC::exmap varRenaming;
            for (const ExprSymbol &x: allVars) {
                varRenaming[x] = varMan.getVarSymbol(varMan.addFreshVariable(x.get_name()));
            }
            z3::expr_vector renamed(context);
            for (Expression e: g) {
                e.applySubs(varRenaming);
                renamed.push_back(e.toZ3(context));
            }
            for (Expression t: templates) {
                t.applySubs(varRenaming);
                renamed.push_back(t.toZ3(context));
            }
            for (Expression e: premise) {
                e.applySubs(varRenaming);
                renamed.push_back(e.toZ3(context));
            }
            const z3::expr &expr = mk_and(renamed);
            soft.push_back(expr);
            someSat.push_back(expr);
        }
        return {{validImplication, z3::mk_or(someSat)}, soft};
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

    const option<pair<vector<z3::expr>, vector<z3::expr>>> buildHardAndSoftConstraints(
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
                        conclusion,
                        varMan);
            }
        }
        return {};
    }

    const ExprSymbolSet findRelevantVariables(const VarMan &varMan, const Expression &c, const Rule &r) {
        set<VariableIdx> res;
        // Add all variables appearing in the guard
        const ExprSymbolSet &guardVariables = c.getVariables();
        for (const ExprSymbol &sym : guardVariables) {
            res.insert(varMan.getVarIdx(sym));
        }
        // Compute the closure of res under all updates and the guard
        set<VariableIdx> todo = res;
        while (!todo.empty()) {
            ExprSymbolSet next;
            for (const VariableIdx &var : todo) {
                for (const RuleRhs &rhs: r.getRhss()) {
                    const UpdateMap &update = rhs.getUpdate();
                    auto it = update.find(var);
                    if (it != update.end()) {
                        const ExprSymbolSet &rhsVars = it->second.getVariables();
                        next.insert(rhsVars.begin(), rhsVars.end());
                    }
                }
                for (const Expression &g: r.getGuard()) {
                    const ExprSymbolSet &gVars = g.getVariables();
                    if (gVars.find(varMan.getVarSymbol(var)) != gVars.end()) {
                        next.insert(gVars.begin(), gVars.end());
                    }
                }
            }
            todo.clear();
            for (const ExprSymbol &sym : next) {
                const VariableIdx &var = varMan.getVarIdx(sym);
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

    const GuardList findRelevantConstraints(const GuardList &guard, const ExprSymbolSet &vars) {
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

    const pair<GuardList, GuardList> splitInitiallyValid(
            const vector<GuardList> precondition,
            const GuardList &invariants) {
        Z3Context context;
        Z3Solver solver(context);
        GuardList initiallyValid;
        GuardList notInitiallyValid;
        z3::expr_vector preconditionVec(context);
        for (const GuardList &pre: precondition) {
            z3::expr_vector preVec(context);
            for (const Expression &e: pre) {
                preVec.push_back(e.toZ3(context));
            }
            preconditionVec.push_back(z3::mk_and(preVec));
        }
        solver.add(z3::mk_or(preconditionVec));
        for (const Expression &i: invariants) {
            solver.push();
            solver.add(!i.toZ3(context));
            if (solver.check() == z3::unsat) {
                initiallyValid.push_back(i);
            } else {
                notInitiallyValid.push_back(i);
            }
            solver.pop();
        }
        return {initiallyValid, notInitiallyValid};
    }

    const pair<GuardList, GuardList> tryToForceInvariance(
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
            const ExprSymbolSet &varSymbols = findRelevantVariables(varMan, g, r);
            allRelevantVariables.insert(varSymbols.begin(), varSymbols.end());
            const pair<Expression, ExprSymbolSet> &p = buildTemplate(varSymbols, varMan);
            templates.emplace_back(p.first <= 0);
            templateParams.insert(p.second.begin(), p.second.end());
        }
        const GuardList &relevantConstraints = findRelevantConstraints(r.getGuard(), allRelevantVariables);
        const optional<pair<vector<z3::expr>, vector<z3::expr>>> &smtExpressions = buildHardAndSoftConstraints(
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
            const vector<z3::expr> &hard = smtExpressions.get().first;
            const vector<z3::expr> &soft = smtExpressions.get().second;
            const option<z3::model> &model = solver.maxSMT(hard, soft);
            if (model) {
                const GuardList &newInvariants = instantiateTemplates(
                        templates,
                        templateParams,
                        varMan,
                        model.get(),
                        context);
                for (const Expression &e: newInvariants) {
                    debugInvariants("new invariant " << e);
                }
                return splitInitiallyValid(preconditions, newInvariants);
            }
        }
        return {{}, {}};
    }

    const vector<GuardList> buildPreconditions(
            const vector<Rule> &predecessors,
            const Rule &r,
            VariableManager &varMan) {
        vector<GuardList> res;
        GiNaC::exmap tmpVarRenaming;
        for (const VariableIdx &i: varMan.getTempVars()) {
            const ExprSymbol &x = varMan.getVarSymbol(i);
            tmpVarRenaming[x] = varMan.getVarSymbol(varMan.addFreshVariable(x.get_name()));
        }
        for (const Rule &pred: predecessors) {
            if (pred.getGuard() == r.getGuard()) {
                continue;
            }
            for (const RuleRhs &rhs: pred.getRhss()) {
                if (rhs.getLoc() == r.getLhsLoc()) {
                    const GiNaC::exmap &up = rhs.getUpdate().toSubstitution(varMan);
                    GuardList pre;
                    for (Expression g: pred.getGuard()) {
                        g.applySubs(up);
                        g.applySubs(tmpVarRenaming);
                        pre.push_back(g);
                    }
                    for (const auto &p: rhs.getUpdate()) {
                        const ExprSymbol &var = varMan.getVarSymbol(p.first);
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

}

const vector<Rule> Strengthening::apply(
        const vector<Rule> &predecessors,
        const Rule &r,
        VariableManager &varMan) {
    vector<Rule> res;
    if (!r.isSimpleLoop()) {
        return res;
    }
    const pair<GuardList, GuardList> &p1 = splitInvariants(r, varMan);
    const GuardList &invariants = p1.first;
    const GuardList &nonInvariants = p1.second;
    const pair<GuardList, GuardList> &p2 = splitMonotonicConstraints(r, invariants, nonInvariants, varMan);
    const GuardList &monotonic = p2.first;
    const GuardList &nonMonotonic = p2.second;
    if (!nonMonotonic.empty()) {
        const vector<GuardList> &preconditions = buildPreconditions(predecessors, r, varMan);
        GuardList todo;
        for (const Expression &e: nonMonotonic) {
            if (Relation::isLinearEquality(e)) {
                todo.emplace_back(e.lhs() <= e.rhs());
                todo.emplace_back(e.rhs() <= e.lhs());
            } else if (Relation::isLinearInequality(e)) {
                todo.push_back(e);
            }
        }
        const pair<GuardList, GuardList> &p3 = tryToForceInvariance(r, todo, preconditions, varMan);
        const GuardList &newInvariants = p3.first;
        const GuardList &newPseudoInvariants = p3.second;
        if (!newInvariants.empty() || !newPseudoInvariants.empty()) {
            GuardList newGuard(r.getGuard());
            for (const Expression &g: newInvariants) {
                newGuard.push_back(g);
            }
            GuardList pseudoInvariantsValid(newGuard);
            for (const Expression &g: newPseudoInvariants) {
                pseudoInvariantsValid.push_back(g);
            }
            res.emplace_back(Rule(RuleLhs(r.getLhsLoc(), pseudoInvariantsValid, r.getCost()), r.getRhss()));
            for (const Expression &e: newPseudoInvariants) {
                GuardList pseudoInvariantInvalid(newGuard);
                assert(Relation::isInequality(e));
                const Expression &negated = Relation::negateLessEqInequality(Relation::toLessEq(e));
                debugTest("deduced pseudo-invariant " << e << ", also trying " << negated);
                pseudoInvariantInvalid.push_back(negated);
                res.emplace_back(Rule(RuleLhs(r.getLhsLoc(), pseudoInvariantInvalid, r.getCost()), r.getRhss()));
            }
        }
    }
    return res;
}
