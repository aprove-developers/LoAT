#include "backwardacceleration.h"

#include "debug.h"
#include "z3/z3solver.h"
#include "z3/z3toolbox.h"

#include "recurrence/dependencyorder.h"
#include "recurrence/recurrence.h"
#include "meter/metertools.h"
#include "expr/guardtoolbox.h"
#include "expr/relation.h"

#include <purrs.hh>

namespace Purrs = Parma_Recurrence_Relation_Solver;
using namespace std;
using boost::optional;

BackwardAcceleration::BackwardAcceleration(VarMan &varMan, const LinearRule &rule)
        : varMan(varMan), rule(rule)
{}


bool BackwardAcceleration::shouldAccelerate() const {
    return rule.getCost().isPolynomial();
}


optional<GiNaC::exmap> BackwardAcceleration::computeInverseUpdate(const vector<VariableIdx> &order) const {
    // Gather guard variables
    set<VariableIdx> relevantVars;
    for (const Expression &ex : rule.getGuard()) {
        for (const ExprSymbol &var : ex.getVariables()) {
            relevantVars.insert(varMan.getVarIdx(var));
        }
    }

    // we also need to know the inverse update for every variable that occurs in the update of a relevant variable
    const UpdateMap &update = rule.getUpdate();
    bool changed;
    do {
        changed = false;
        for (VariableIdx var : relevantVars) {
            auto it = update.find(var);
            if (it != update.end()) {
                for (const ExprSymbol &rhsVar : it->second.getVariables()) {
                    changed = relevantVars.insert(varMan.getVarIdx(rhsVar)).second || changed;
                }
            }
        }
    } while (changed);

    // Compute the inverse update for all relevant variables, in the given order.
    // Given e.g. x' = x+3, we basically solve for x and get x/x-3 as inverse update.
    // We have to be careful if other variables appear in the update (e.g. x' = x+y or x' = y).

    const GiNaC::exmap &updateSubs = update.toSubstitution(varMan);
    GiNaC::exmap inverseUpdate;

    for (VariableIdx var : order) {
        if (relevantVars.count(var) == 0) {
            continue;
        }
        assert(update.count(var) > 0); // order only contains updated variables

        ExprSymbol x = varMan.getGinacSymbol(var);
        Expression rhs = update.at(var);
        Expression inverseRhs;

        if (rhs.degree(x) > 1) {
            debugBackwardAccel("update " << rhs << " is not linear (in its left-hand side " << x << ")");
            return {};
        }

        // Distinguish 3 cases as in the paper, for x := alpha*x + beta.
        Expression alpha = rhs.coeff(x, 1);
        Expression beta = rhs.coeff(x, 0);
        assert(rhs.is_equal(alpha * x + beta));

        // If x does not occur in update(x), then we know how to compute the inverse udpate in some cases...
        if (alpha.is_zero()) {
            // ...e.g., if update(update(x)) = update(x)...
            if (rhs.subs(updateSubs) == rhs) {
                inverseRhs = rhs;

            // ...and if update(inverse_update(update(x))) = update(x)...
            } else if (rhs.subs(inverseUpdate).subs(updateSubs).is_equal(rhs)) {
                inverseRhs = rhs.subs(inverseUpdate).subs(inverseUpdate);

            // ...but in all other cases, we have no idea.
            } else {
                debugBackwardAccel("don't know how to inverse update " << rhs << " for variable " << x);
                return {};
            }

        // We also know how to compute the inverse update if x's coefficient is a constant
        } else if (alpha.isRationalConstant()) {
            inverseRhs = (x - beta.subs(inverseUpdate)) / alpha;

        } else {
            debugBackwardAccel("update " << rhs << " has non-constant coefficient for " << x);
            return {};
        }

        // Computation of inverse update was successful for x
        inverseUpdate[x] = inverseRhs;
    }

    debugBackwardAccel("successfully computed inverse update " << inverseUpdate);
    return inverseUpdate;
}


bool BackwardAcceleration::checkGuardImplication(const GiNaC::exmap &inverseUpdate) const {
    Z3Context context;
    Z3Solver solver(context);

    // Remove constraints that are irrelevant for the loop's execution
    GuardList reducedGuard = MeteringToolbox::reduceGuard(varMan, rule.getGuard(), {rule.getUpdate()});

    // Build the implication by applying the inverse update to every guard constraint.
    // For the left-hand side, we use the full guard (might be stronger than the reduced guard).
    // For the right-hand side, we only check the reduced guard, as we only care about relevant constraints.
    vector<z3::expr> lhss, rhss;

    for (const Expression &ex : rule.getGuard()) {
        lhss.push_back(GinacToZ3::convert(ex, context));
    }
    z3::expr lhs = Z3Toolbox::concat(context, lhss, Z3Toolbox::ConcatAnd);

    for (const Expression &ex : reducedGuard) {
        rhss.push_back(GinacToZ3::convert(ex.subs(inverseUpdate), context));
    }
    z3::expr rhs = Z3Toolbox::concat(context, rhss, Z3Toolbox::ConcatAnd);

    // call z3
    debugBackwardAccel("Checking guard implication:  " << lhs << "  ==>  " << rhs);
    solver.add(!rhs && lhs);
    return (solver.check() == z3::unsat);
}


LinearRule BackwardAcceleration::buildAcceleratedRule(const UpdateMap &iteratedUpdate,
                                                      const Expression &iteratedCost,
                                                      const ExprSymbol &N) const
{
    GiNaC::exmap updateSubs = iteratedUpdate.toSubstitution(varMan);

    // Extend the old guard by the updated constraints
    // and require that the number of iterations N is positive
    GuardList newGuard = rule.getGuard();
    newGuard.push_back(N > 0);
    for (const Expression &ex : rule.getGuard()) {
        newGuard.push_back(ex.subs(updateSubs).subs(N == N-1)); // apply the update N-1 times
    }

    LinearRule res(rule.getLhsLoc(), newGuard, iteratedCost, rule.getRhsLoc(), iteratedUpdate);
    debugBackwardAccel("backward-accelerating " << rule << " yielded " << res);
    return res;
}


// TODO: Refactor this to be more general, can then also be used for free var instantiation (see metertools)
// TODO: A general method can return a list of <Expression,BoundType> with BoundType = { Lower, Upper, Equality }.
optional<vector<Expression>> BackwardAcceleration::computeUpperbounds(const ExprSymbol &N, const GuardList &guard) {
    // First check if there is an equality constraint (we can then ignore all other upper bounds)
    for (const Expression &ex : guard) {
        if (Relation::isEquality(ex) && ex.has(N)) {
            auto optSolved = GuardToolbox::solveTermFor(ex.lhs()-ex.rhs(), N, GuardToolbox::ResultMapsToInt);
            if (!optSolved) {
                debugBackwardAccel("unable to compute upperbound from equality " << ex);
                return {};
            }
            // One equality is enough, as all other bounds must also satisfy this equality
            return vector<Expression>({optSolved.get()});
        }
    }

    // Otherwise, collect all upper bounds
    vector<Expression> bounds;
    for (const Expression &ex : guard) {
        if (Relation::isEquality(ex) || !ex.has(N)) continue;

        Expression term = Relation::toLessEq(ex);
        term = term.lhs() - term.rhs();
        if (term.degree(N) != 1) continue;

        // ignore lower bounds (terms of the form -N <= 0)
        if (term.coeff(N, 1).info(GiNaC::info_flags::negative)) {
            continue;
        }

        // compute the upper bound represented by N and check that it is integral
        auto optSolved = GuardToolbox::solveTermFor(term, N, GuardToolbox::ResultMapsToInt);
        if (!optSolved) {
            debugBackwardAccel("unable to compute upperbound from " << ex);
            return {};
        }
        bounds.push_back(optSolved.get());
    }

    if (bounds.empty()) {
        debugBackwardAccel("warning: no upperbounds found, not instantiating " << N);
        return {};
    }

    return bounds;
}


vector<LinearRule> BackwardAcceleration::replaceByUpperbounds(const ExprSymbol &N, const LinearRule &rule) {
    // gather all upper bounds (if possible)
    auto bounds = computeUpperbounds(N, rule.getGuard());

    // avoid rule explosion (by not instantiating N if there are too many bounds)
    if (!bounds || bounds.get().size() > BACKWARD_ACCEL_MAXBOUNDS) {
        return {rule};
    }

    // create one rule for each upper bound, by instantiating N with this bound
    vector<LinearRule> res;
    for (const Expression &bound : bounds.get()) {
        GiNaC::exmap subs;
        subs[N] = bound;

        LinearRule instantiated = rule;
        instantiated.applySubstitution(subs);
        res.push_back(instantiated);
        debugBackwardAccel("instantiation " << subs << " yielded " << instantiated);
    }
    return res;
}


optional<vector<LinearRule>> BackwardAcceleration::run() {
    if (!shouldAccelerate()) {
        debugBackwardAccel("won't try to accelerate transition with costs " << rule.getCost());
        return {};
    }
    debugBackwardAccel("Trying to accelerate rule " << rule);

    auto order = DependencyOrder::findOrder(varMan, rule.getUpdate());
    if (!order) {
        debugBackwardAccel("failed to compute dependency order for rule " << rule);
        return {};
    }

    auto inverseUpdate = computeInverseUpdate(order.get());
    if (!inverseUpdate) {
        debugBackwardAccel("Failed to compute inverse update");
        return {};
    }

    if (!checkGuardImplication(inverseUpdate.get())) {
        debugBackwardAccel("Failed to check guard implication");
        return {};
    }

    // compute the iterated update and cost, with a fresh variable N as iteration step
    ExprSymbol N = varMan.getGinacSymbol(varMan.addFreshTemporaryVariable("k"));

    UpdateMap iteratedUpdate = rule.getUpdate();
    Expression iteratedCost = rule.getCost();
    if (!Recurrence::iterateUpdateAndCost(varMan, iteratedUpdate, iteratedCost, N)) {
        debugBackwardAccel("Failed to compute iterated cost/update");
        return {};
    }

    // compute the resulting rule and try to simplify it by instantiating N
    LinearRule accelerated = buildAcceleratedRule(iteratedUpdate, iteratedCost, N);
    return replaceByUpperbounds(N, accelerated);
}


optional<vector<LinearRule>> BackwardAcceleration::accelerate(VarMan &varMan, const LinearRule &rule) {
    BackwardAcceleration ba(varMan, rule);
    return ba.run();
}
