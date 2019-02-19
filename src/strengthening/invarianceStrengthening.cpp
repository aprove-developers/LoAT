//
// Created by ffrohn on 2/8/19.
//

#include <expr/relation.h>
#include <accelerate/meter/farkas.h>
#include "invarianceStrengthening.h"
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
        const GuardList &todo,
        VariableManager &varMan
): templates(templates),
   templateParams(templateParams),
   relevantConstraints(relevantConstraints),
   relevantVars(relevantVars),
   updates(updates),
   preconditions(preconditions),
   todo(todo),
   varMan(varMan),
   z3Context(new Z3Context()){ }

const option<Types::Invariants> Self::apply() {
    const option<Types::SMTConstraints> &smtConstraints = buildSMTConstraints();
    if (smtConstraints) {
        return solve(smtConstraints.get());
    } else {
        return {};
    }
}

const option<Types::SMTConstraints> Self::buildSMTConstraints() const {
    GuardList premise;
    GuardList conclusion;
    for (const Expression &e: relevantConstraints) {
        premise.push_back(e);
    }
    for (const Expression &g: todo) {
        for (const GiNaC::exmap &up: updates) {
            Expression updated = g;
            updated.applySubs(up);
            if (Relation::isLinearInequality(updated, relevantVars)) {
                conclusion.push_back(updated);
            }
        }
    }
    if (!conclusion.empty()) {
        const Types::Implication &templatesInvariantImplication = buildTemplatesInvariantImplication();
        if (!templatesInvariantImplication.premise.empty()) {
            Types::SMTConstraints res;
            const Types::SMTConstraints &initiation = constructZ3Initiation(premise);
            for (const z3::expr &e: initiation.soft) {
                res.soft.push_back(e);
            }
            for (const z3::expr &e: initiation.hard) {
                res.hard.push_back(e);
            }
            for (const Expression &e: templatesInvariantImplication.premise) {
                premise.push_back(e);
            }
            const std::vector<z3::expr> templatesInvariant = constructZ3Implication(
                    premise,
                    templatesInvariantImplication.conclusion);
            for (const z3::expr &e: templatesInvariant) {
                res.hard.push_back(e);
            }
            const std::vector<z3::expr> &conclusionInvariant = constructZ3Implication(premise, conclusion);
            z3::expr_vector v(*z3Context);
            for (const z3::expr &e: conclusionInvariant) {
                v.push_back(e);
                res.soft.push_back(e);
            }
            res.hard.push_back(z3::mk_or(v));
            return res;
        }
    }
    return {};
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

const Types::SMTConstraints Self::constructZ3Initiation(const GuardList &premise) const {
    Types::SMTConstraints res;
    z3::expr_vector someSat(*z3Context);
    for (const GuardList &pre: preconditions) {
        z3::expr_vector gVec(*z3Context);
        for (const Expression &e: pre) {
            gVec.push_back(e.toZ3(*z3Context));
        }
        for (const Expression &t: templates) {
            res.soft.push_back(FarkasLemma::apply(
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
        res.soft.push_back(expr);
        someSat.push_back(expr);
    }
    res.hard.push_back(z3::mk_or(someSat));
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

const option<Types::Invariants> Self::solve(const Types::SMTConstraints &smtConstraints) const {
    Z3Solver solver(*z3Context);
    const option<z3::model> &model = solver.maxSMT(smtConstraints.hard, smtConstraints.soft);
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
