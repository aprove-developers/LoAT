//
// Created by ffrohn on 2/8/19.
//

#include <expr/relation.h>
#include <accelerate/meter/farkas.h>
#include "invariancestrengthening.h"
#include <z3/z3solver.h>
#include <z3/z3toolbox.h>

typedef StrengtheningTypes Types;
typedef InvarianceStrengthening Self;

Self::InvarianceStrengthening(
        const std::vector<Expression> &templates,
        const ExprSymbolSet &templateParams,
        const GuardList &relevantConstraints,
        const ExprSymbolSet &relevantVars,
        const std::vector<GiNaC::exmap> &updates,
        const std::vector<GuardList> &preconditions,
        const GuardList &invariants,
        const GuardList &todo,
        const StrengtheningMode::Mode &mode,
        VariableManager &varMan
): templates(templates),
   templateParams(templateParams),
   relevantConstraints(relevantConstraints),
   relevantVars(relevantVars),
   updates(updates),
   preconditions(preconditions),
   invariants(invariants),
   todo(todo),
   mode(mode),
   varMan(varMan),
   z3Context(new Z3Context()){
}

const option<Types::Invariants> Self::apply() {
    const Types::SmtConstraints &smtConstraints = buildSMTConstraints();
    Types::MaxSmtConstraints maxSmtConstraints = mode(smtConstraints, *z3Context);
    if (maxSmtConstraints.hard.empty()) {
        return {};
    } else {
        return solve(maxSmtConstraints);
    }
}

const Types::SmtConstraints Self::buildSMTConstraints() const {
    GuardList invariancePremise;
    GuardList invarianceConclusion;
    GuardList monotonicityPremise;
    GuardList monotonicityConclusion;
    invariancePremise.insert(invariancePremise.end(), relevantConstraints.begin(), relevantConstraints.end());
    for (const Expression &e: todo) {
        for (const GiNaC::exmap &up: updates) {
            Expression updated = e;
            updated.applySubs(up);
            if (Relation::isLinearInequality(updated, relevantVars)) {
                invarianceConclusion.push_back(updated);
            }
        }
    }
    for (const Expression &e: relevantConstraints) {
        for (const GiNaC::exmap &up: updates) {
            Expression updated = e;
            updated.applySubs(up);
            monotonicityPremise.push_back(updated);
        }
    }
    for (const Expression &e: templates) {
        for (const GiNaC::exmap &up: updates) {
            Expression updated = e;
            updated.applySubs(up);
            monotonicityPremise.push_back(updated);
        }
    }
    monotonicityPremise.insert(monotonicityPremise.begin(), templates.begin(), templates.end());
    monotonicityPremise.insert(monotonicityPremise.begin(), invariants.begin(), invariants.end());
    for (const Expression &e: todo) {
        if (Relation::isLinearInequality(e, relevantVars)) {
            monotonicityConclusion.push_back(e);
        }
    }
    const Types::Implication &templatesInvariantImplication = buildTemplatesInvariantImplication();
    // We use templatesInvariantImplication.premise instead of templates as buildTemplatesInvariantImplication discards
    // templates that become non-linear when applying the update.
    invariancePremise.insert(invariancePremise.begin(), templatesInvariantImplication.premise.begin(), templatesInvariantImplication.premise.end());
    const Types::Initiation &initiation = constructZ3Initiation(relevantConstraints);
    const std::vector<z3::expr> templatesInvariant = constructZ3Implication(
            invariancePremise,
            templatesInvariantImplication.conclusion);
    const std::vector<z3::expr> &conclusionInvariant = constructZ3Implication(invariancePremise, invarianceConclusion);
    const std::vector<z3::expr> &conclusionMonotonic = constructZ3Implication(monotonicityPremise, monotonicityConclusion);
    return Types::SmtConstraints(initiation, templatesInvariant, conclusionInvariant, conclusionMonotonic);
}

const Types::Implication Self::buildTemplatesInvariantImplication() const {
    Types::Implication res;
    for (const Expression &invTemplate: templates) {
        bool linear = true;
        GuardList updatedTemplates;
        for (const GiNaC::exmap &up: updates) {
            Expression updated = invTemplate;
            updated.applySubs(up);
            if (Relation::isLinearInequality(updated, relevantVars)) {
                updatedTemplates.push_back(updated);
            } else {
                linear = false;
                break;
            }
        }
        if (linear) {
            res.premise.push_back(invTemplate);
            for (const Expression &t: updatedTemplates) {
                res.conclusion.push_back(t);
            }
        }
    }
    return res;
}

const Types::Initiation Self::constructZ3Initiation(const GuardList &premise) const {
    Types::Initiation res;
    for (const GuardList &pre: preconditions) {
        z3::expr_vector gVec(*z3Context);
        for (const Expression &e: pre) {
            gVec.push_back(e.toZ3(*z3Context));
        }
        for (const Expression &t: templates) {
            res.valid.push_back(FarkasLemma::apply(
                    pre,
                    t,
                    relevantVars,
                    templateParams,
                    *z3Context,
                    Z3Context::Integer));
        }
        ExprSymbolSet allVars;
        pre.collectVariables(allVars);
        for (const Expression &e: premise) {
            e.collectVariables(allVars);
        }
        for (const Expression &e: pre) {
            e.collectVariables(allVars);
        }
        GiNaC::exmap varRenaming;
        for (const ExprSymbol &x: allVars) {
            varRenaming[x] = varMan.getVarSymbol(varMan.addFreshVariable(x.get_name()));
        }
        z3::expr_vector renamed(*z3Context);
        for (Expression e: pre) {
            e.applySubs(varRenaming);
            renamed.push_back(e.toZ3(*z3Context));
        }
        for (Expression t: templates) {
            t.applySubs(varRenaming);
            renamed.push_back(t.toZ3(*z3Context));
        }
        for (Expression e: premise) {
            e.applySubs(varRenaming);
            renamed.push_back(e.toZ3(*z3Context));
        }
        const z3::expr &expr = mk_and(renamed);
        res.satisfiable.push_back(expr);
    }
    return res;
}

const std::vector<z3::expr> Self::constructZ3Implication(
        const GuardList &premise,
        const GuardList &conclusion) const {
    return FarkasLemma::apply(
            premise,
            conclusion,
            relevantVars,
            templateParams,
            *z3Context,
            Z3Context::Integer);
}

const option<Types::Invariants> Self::solve(const Types::MaxSmtConstraints &constraints) const {
    Z3Solver solver(*z3Context);
    const option<z3::model> &model = solver.maxSmt(constraints.hard, constraints.soft);
    if (model) {
        const GuardList &newInvariants = instantiateTemplates(model.get());
        if (!newInvariants.empty()) {
            for (const Expression &e: newInvariants) {
                debugInvariants("new invariant " << e);
            }
            return splitInitiallyValid(preconditions, newInvariants);
        }
    }
    return {};
}

const GuardList Self::instantiateTemplates(const z3::model &model) const {
    GuardList res;
    UpdateMap parameterInstantiation;
    for (const ExprSymbol &p: templateParams) {
        const option<z3::expr> &var = z3Context->getVariable(p);
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
            if (templateParams.find(var) != templateParams.end()) {
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

const Types::Invariants Self::splitInitiallyValid(
        const std::vector<GuardList> &precondition,
        const GuardList &invariants) const {
    Z3Solver solver(*z3Context);
    Types::Invariants res;
    z3::expr_vector preconditionVec(*z3Context);
    for (const GuardList &pre: precondition) {
        z3::expr_vector preVec(*z3Context);
        for (const Expression &e: pre) {
            preVec.push_back(e.toZ3(*z3Context));
        }
        preconditionVec.push_back(z3::mk_and(preVec));
    }
    solver.add(z3::mk_or(preconditionVec));
    for (const Expression &i: invariants) {
        solver.push();
        solver.add(!i.toZ3(*z3Context));
        if (solver.check() == z3::unsat) {
            res.invariants.push_back(i);
        } else {
            res.pseudoInvariants.push_back(i);
        }
        solver.pop();
    }
    return res;
}
