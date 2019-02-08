//
// Created by ffrohn on 2/8/19.
//

#include <expr/relation.h>
#include <accelerate/meter/farkas.h>
#include "strengthening.h"
#include "z3/z3solver.h"
#include "z3/z3toolbox.h"
#include "metertools.h"

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

static pair<Expression, vector<ExprSymbol>> buildTemplate(
        const vector<ExprSymbol > &vars,
        VariableManager &varMan) {
    vector<ExprSymbol> coeffs;
    ExprSymbol c0 = varMan.getVarSymbol(varMan.addFreshVariable("c0"));
    coeffs.push_back(c0);
    Expression lhs = Expression(0);
    for (const ExprSymbol &x: vars) {
        ExprSymbol coeff = varMan.getVarSymbol(varMan.addFreshVariable("c"));
        coeffs.push_back(coeff);
        lhs = lhs + (x * coeff);
    }
    Expression res = lhs <= c0;
    return pair<Expression, vector<ExprSymbol>>(res, coeffs);
}

static pair<GuardList, GuardList> tryToForceInvariance(
        const Rule &r,
        const GuardList &todo,
        VariableManager &varMan) {
    Z3Context context;
    Z3Solver solver(context);
    GuardList invariants;
    GuardList fail;
    GuardList premise;
    GuardList conclusion;
    for (const Expression &g: r.getGuard()) {
        debugBackwardAccel("adding " << g << " to premise");
        premise.push_back(Relation::splitVariablesAndConstants(Relation::toLessEq(g)));
    }
    vector<UpdateMap> multiUpdate;
    vector<GiNaC::exmap> updates;
    for (const RuleRhs &u: r.getRhss()) {
        multiUpdate.push_back(u.getUpdate());
        updates.push_back(u.getUpdate().toSubstitution(varMan));
    }
    set<VariableIdx> vars = MeteringToolbox::findRelevantVariables(varMan, r.getGuard(), multiUpdate);
    vector<ExprSymbol> varSymbols;
    for (const VariableIdx &x: vars) {
        varSymbols.push_back(varMan.getVarSymbol(x));
    }
    for (const Expression &g: todo) {
        for (const GiNaC::exmap &up: updates) {
            Expression updated = g;
            updated.applySubs(up);
            debugBackwardAccel("adding " << updated << " to conclusion");
            conclusion.push_back(Relation::splitVariablesAndConstants(Relation::toLessEq(updated)));
        }
        vector<ExprSymbol> allTemplateParams;
        vector<Expression> templates;
        for (int i = 0; i < Config::Invariants::NumTemplates; i++) {
            pair<Expression, vector<ExprSymbol>> p = buildTemplate(varSymbols, varMan);
            Expression invTemplate = p.first;
            vector<ExprSymbol> curTemplateParams = p.second;
            templates.push_back(invTemplate);
            for (const ExprSymbol &param: curTemplateParams) {
                allTemplateParams.push_back(param);
            }
            debugBackwardAccel("adding " << invTemplate << " to premise");
            premise.push_back(invTemplate);
            for (const GiNaC::exmap &up: updates) {
                Expression updated = invTemplate;
                updated.applySubs(up);
                debugBackwardAccel("adding " << updated << " to conclusion");
                conclusion.push_back(Relation::splitVariablesAndConstants(updated, curTemplateParams));
            }
        }
        debugBackwardAccel("premise:");
        for (const Expression &p: premise) {
            debugBackwardAccel(p);
        }
        for (const Expression &c: conclusion) {
            vector<z3::expr> coeffs;
            for (auto i = varSymbols.rbegin(); i != varSymbols.rend(); i++) {
                coeffs.push_back(Expression(c.lhs().coeff(*i, 1)).toZ3(context));
            }
            debugBackwardAccel("conclusion: " << c);
            dumpList("varSymbols", varSymbols);
            dumpList("coeffs", coeffs);
            debugBackwardAccel("c0: " << -Expression(c.rhs()));
            z3::expr farkas = FarkasLemma::apply(premise, varSymbols, coeffs, -Expression(c.rhs()).toZ3(context), 0, context, allTemplateParams);
            debugBackwardAccel("farkas: " << farkas);
            solver.add(farkas);
        }
        debugBackwardAccel("solver: " << solver);
        z3::check_result res = solver.check();
        debugBackwardAccel("res: " << res);
        if (res == z3::check_result::sat) {
            invariants.push_back(g);
            z3::model model = solver.get_model();
            debugBackwardAccel("found invariant; model: " << model);
            debugBackwardAccel("template params:");
            UpdateMap templateInstantiation;
            for (const ExprSymbol &p: allTemplateParams) {
                Expression instantiation = Z3Toolbox::getRealFromModel(model, context.getVariable(p).get());
                templateInstantiation.emplace(varMan.getVarIdx(p), instantiation);
            }
            GiNaC::exmap subst = templateInstantiation.toSubstitution(varMan);
            debugBackwardAccel("found invariant; template instantiation: " << subst);
            for (Expression t: templates) {
                debugBackwardAccel("template: " << t);
                t.applySubs(subst);
                debugBackwardAccel("deduced invariant " << t);
                invariants.push_back(t);
            }
        } else {
            fail.push_back(g);
        }
    }
    return pair<GuardList, GuardList>(invariants, fail);
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
    GuardList fail = p.second;
    if (newInvariants.empty()) {
        return optional<Rule>{};
    } else {
        GuardList newGuard;
        for (const Expression &g: invariants) {
            newGuard.push_back(g);
        }
        for (const Expression &g: newInvariants) {
            newGuard.push_back(g);
        }
        for (const Expression &g: fail) {
            newGuard.push_back(g);
        }
        return Rule(RuleLhs(r.getLhsLoc(), newGuard, r.getCost()), r.getRhss());
    }
}
