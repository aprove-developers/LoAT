//
// Created by ffrohn on 2/21/19.
//

#include "../util/option.hpp"
#include "../z3/z3solver.hpp"
#include "../z3/z3toolbox.hpp"
#include "constraintsolver.hpp"
#include "../its/variablemanager.hpp"

namespace strengthening {

    typedef ConstraintSolver Self;

    const option<Invariants> Self::solve(
            const RuleContext &ruleCtx,
            const MaxSmtConstraints &constraints,
            const Templates &templates,
            Z3Context &z3Ctx) {
        return ConstraintSolver(ruleCtx, constraints, templates, z3Ctx).solve();
    }

    Self::ConstraintSolver(
            const RuleContext &ruleCtx,
            const MaxSmtConstraints &constraints,
            const Templates &templates,
            Z3Context &z3Ctx):
            ruleCtx(ruleCtx),
            constraints(constraints),
            templates(templates),
            z3Ctx(z3Ctx) { }

    const option<Invariants> Self::solve() const {
        Z3Solver solver(z3Ctx, Config::Z3::StrengtheningTimeout);
        const option<z3::model> &model = solver.maxSmt(constraints.hard, constraints.soft);
        if (model) {
            const GuardList &newInvariants = instantiateTemplates(model.get());
            if (!newInvariants.empty()) {
                return splitInitiallyValid(newInvariants);
            }
        }
        return {};
    }

    const GuardList Self::instantiateTemplates(const z3::model &model) const {
        Z3Solver solver(z3Ctx);
        GuardList res;
        UpdateMap parameterInstantiation;
        for (const ExprSymbol &p: templates.params()) {
            const option<z3::expr> &var = z3Ctx.getVariable(p);
            if (var) {
                const Expression &pi = Z3Toolbox::getRealFromModel(model, var.get());
                parameterInstantiation.emplace(ruleCtx.varMan.getVarIdx(p), pi);
            }
        }
        const GiNaC::exmap &subs = parameterInstantiation.toSubstitution(ruleCtx.varMan);
        const std::vector<Expression> instantiatedTemplates = templates.subs(subs);
        for (const Expression &e: instantiatedTemplates) {
            if (!templates.isParametric(e)) {
                solver.add(!e.toZ3(z3Ctx));
                if (solver.check() != z3::unsat) {
                    res.push_back(e);
                }
                solver.reset();
            }
        }
        return res;
    }

    const option<Invariants> Self::splitInitiallyValid(const GuardList &invariants) const {
        Z3Solver solver(z3Ctx);
        Invariants res;
        z3::expr_vector preconditionVec(z3Ctx);
        for (const GuardList &pre: ruleCtx.preconditions) {
            z3::expr_vector preVec(z3Ctx);
            for (const Expression &e: pre) {
                preVec.push_back(e.toZ3(z3Ctx));
            }
            preconditionVec.push_back(z3::mk_and(preVec));
        }
        solver.add(z3::mk_or(preconditionVec));
        for (const Expression &i: invariants) {
            if (Timeout::soft()) {
                return {};
            }
            solver.push();
            solver.add(!i.toZ3(z3Ctx));
            if (solver.check() == z3::unsat) {
                res.invariants.push_back(i);
            } else {
                res.pseudoInvariants.push_back(i);
            }
            solver.pop();
        }
        return res;
    }

}
