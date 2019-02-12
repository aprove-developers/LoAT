#include "backward.h"

#include "debug.h"
#include "z3/z3solver.h"
#include "z3/z3toolbox.h"

#include "recurrence/recurrence.h"
#include "meter/metertools.h"
#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "expr/ginactoz3.h"

#include <purrs.hh>
#include <util/stats.h>

using namespace std;

BackwardAcceleration::BackwardAcceleration(VarMan &varMan, const LinearRule &rule)
        : varMan(varMan), rule(rule)
{}


bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPolynomial();
}


bool BackwardAcceleration::checkGuardImplication(const GuardList &reducedGuard, const GuardList &irrelevantGuard) const {
    Z3Context context;
    Z3Solver solver(context);

    // Build the implication by applying the inverse update to every guard constraint.
    // For the left-hand side, we use the full guard (might be stronger than the reduced guard).
    // For the right-hand side, we only check the reduced guard, as we only care about relevant constraints.
    vector<z3::expr> lhss, rhss;

    auto update = rule.getUpdate().toSubstitution(varMan);
    for (const Expression &ex : rule.getGuard()) {
        lhss.push_back(GinacToZ3::convert(ex.subs(update), context));
    }
    for (const Expression &ex : irrelevantGuard) {
        lhss.push_back(GinacToZ3::convert(ex, context));
    }
    z3::expr lhs = Z3Toolbox::concat(context, lhss, Z3Toolbox::ConcatAnd);

    for (const Expression &ex : reducedGuard) {
        rhss.push_back(GinacToZ3::convert(ex, context));
    }
    z3::expr rhs = Z3Toolbox::concat(context, rhss, Z3Toolbox::ConcatAnd);

    // call z3
    debugBackwardAccel("Checking guard implication:  " << lhs << "  ==>  " << rhs);
    solver.add(lhs);
    if (solver.check() != z3::sat) {
        return false;
    }
    solver.reset();
    solver.add(!rhs && lhs);
    return (solver.check() == z3::unsat);
}


LinearRule BackwardAcceleration::buildAcceleratedRule(const UpdateMap &iteratedUpdate,
                                                      const Expression &iteratedCost,
                                                      const GuardList &guard,
                                                      const ExprSymbol &N) const
{
    GiNaC::exmap updateSubs = iteratedUpdate.toSubstitution(varMan);

    // Extend the old guard by the updated constraints
    // and require that the number of iterations N is positive
    GuardList newGuard = guard;
    newGuard.push_back(N > 0);
    for (const Expression &ex : rule.getGuard()) {
        newGuard.push_back(ex.subs(updateSubs).subs(N == N-1)); // apply the update N-1 times
    }

    LinearRule res(rule.getLhsLoc(), newGuard, iteratedCost, rule.getRhsLoc(), iteratedUpdate);
    debugBackwardAccel("backward-accelerating " << rule << " yielded " << res);
    return res;
}


vector<Expression> BackwardAcceleration::computeUpperbounds(const ExprSymbol &N, const GuardList &guard) {
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
        term = (term.lhs() - term.rhs()).expand();
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
    if (bounds.empty() || bounds.size() > Config::BackwardAccel::MaxUpperboundsForPropagation) {
        return {rule};
    }

    // create one rule for each upper bound, by instantiating N with this bound
    vector<LinearRule> res;
    for (const Expression &bound : bounds) {
        GiNaC::exmap subs;
        subs[N] = bound;

        LinearRule instantiated = rule;
        instantiated.applySubstitution(subs);
        res.push_back(instantiated);
        debugBackwardAccel("instantiation " << subs << " yielded " << instantiated);
    }
    return res;
}


vector<LinearRule> BackwardAcceleration::run() {
    if (!shouldAccelerate()) {
        debugBackwardAccel("won't try to accelerate transition with costs " << rule.getCost());
        return {};
    }
    debugBackwardAccel("Trying to accelerate rule " << rule);

    // Remove constraints that are irrelevant for the loop's execution
    GuardList irrelevantGuard;
    GuardList reducedGuard = MeteringToolbox::reduceGuard(varMan, rule.getGuard(), {rule.getUpdate()}, &irrelevantGuard);

    if (!checkGuardImplication(reducedGuard, irrelevantGuard)) {
        debugBackwardAccel("Failed to check guard implication");
        Stats::add(Stats::BackwardNonMonotonic);
        return {};
    }

    // compute the iterated update and cost, with a fresh variable N as iteration step
    ExprSymbol N = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("k"));

    UpdateMap iteratedUpdate = rule.getUpdate();
    Expression iteratedCost = rule.getCost();
    GuardList strengthenedGuard = rule.getGuard();
    if (!Recurrence::iterateUpdateAndCost(varMan, iteratedUpdate, iteratedCost, strengthenedGuard, N)) {
        debugBackwardAccel("Failed to compute iterated cost/update");
        Stats::add(Stats::BackwardCannotIterate);
        return {};
    }
    if (reducedGuard.empty()) {
        iteratedCost = Expression::NontermSymbol;
    }
    Stats::add(Stats::BackwardSuccess);

    // compute the resulting rule and try to simplify it by instantiating N (if enabled)
    LinearRule accelerated = buildAcceleratedRule(iteratedUpdate, iteratedCost, strengthenedGuard, N);
    if (Config::BackwardAccel::ReplaceTempVarByUpperbounds) {
        return replaceByUpperbounds(N, accelerated);
    } else {
        vector<LinearRule> res = {accelerated};
        return res;
    }
}


vector<LinearRule> BackwardAcceleration::accelerate(VarMan &varMan, const LinearRule &rule) {
    BackwardAcceleration ba(varMan, rule);
    return ba.run();
}
