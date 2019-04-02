#include "backward.h"

#include "debug.h"
#include "z3/z3solver.h"
#include "z3/z3toolbox.h"

#include "recurrence/recurrence.h"
#include "meter/metertools.h"
#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "expr/ginactoz3.h"
#include "accelerate/forward.h"

#include <purrs.hh>
#include <util/stats.h>
#include <util/relevantvariables.h>
#include <analysis/chain.h>

using namespace std;

typedef BackwardAcceleration Self;

BackwardAcceleration::BackwardAcceleration(VarMan &varMan, const Rule &rule, const LocationIdx &sink)
        : varMan(varMan), rule(rule), sink(sink) {}


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

    for (const Expression &ex : irrelevantGuard) {
        solver.add(GinacToZ3::convert(ex, context));
    }

    for (const auto &ruleRhs: rule.getRhss()) {
        solver.push();
        auto update = ruleRhs.getUpdate().toSubstitution(varMan);
        for (const Expression &ex : rule.getGuard()) {
            solver.add(GinacToZ3::convert(ex.subs(update), context));
        }
        for (const Expression &ex : reducedGuard) {
            rhss.push_back(GinacToZ3::convert(ex, context));
        }
        z3::expr rhs = Z3Toolbox::concat(context, rhss, Z3Toolbox::ConcatAnd);

        // call z3
        debugBackwardAccel("Checking guard implication:  " << lhs << "  ==>  " << rhs);
        if (solver.check() != z3::sat) {
            return false;
        }
        solver.add(!rhs);
        if (solver.check() != z3::unsat) {
            return false;
        }
        solver.pop();
    }
    return true;
}

Rule BackwardAcceleration::buildNontermRule() const {
    LinearRule res(rule.getLhsLoc(), rule.getGuard(), Expression::NontermSymbol, sink, {});
    debugBackwardAccel("backward-accelerating " << rule << " yielded " << res);
    return std::move(res);
}

Rule BackwardAcceleration::buildAcceleratedLoop(const UpdateMap &iteratedUpdate,
                                                const Expression &iteratedCost,
                                                const GuardList &guard,
                                                const ExprSymbol &N,
                                                const unsigned int validityBound) const
{
    GiNaC::exmap updateSubs = iteratedUpdate.toSubstitution(varMan);

    // Extend the old guard by the updated constraints
    // and require that the number of iterations N is positive
    GuardList newGuard = guard;
    if (validityBound == 0) {
        newGuard.push_back(N >= 0);
    } else {
        newGuard.push_back(N > 0);
    }
    for (const Expression &ex : rule.getGuard()) {
        newGuard.push_back(ex.subs(updateSubs).subs(N == N-1)); // apply the update N-1 times
    }

    LinearRule res(rule.getLhsLoc(), newGuard, iteratedCost, rule.getRhsLoc(0), iteratedUpdate);
    debugBackwardAccel("backward-accelerating " << rule << " yielded " << res);
    return std::move(res);
}

Rule BackwardAcceleration::buildAcceleratedRecursion(
        const std::vector<UpdateMap> &iteratedUpdates,
        const Expression &iteratedCost,
        const GuardList &guard,
        const ExprSymbol &N,
        const unsigned int validityBound) const
{
    GiNaC::exmap updateSubs = iteratedUpdates.begin()->toSubstitution(varMan);
    for (auto it = iteratedUpdates.begin() + 1; it < iteratedUpdates.end(); it++) {
        GiNaC::exmap subs = it->toSubstitution(varMan);
        for (auto &p: updateSubs) {
            updateSubs[p.first] = p.second.subs(subs);
        }
        for (auto &p: subs) {
            if (updateSubs.count(p.first) == 0) {
                updateSubs[p.first] = p.second;
            }
        }
    }

    // Extend the old guard by the updated constraints
    // and require that the number of iterations N is positive
    GuardList newGuard = guard;
    if (validityBound == 0) {
        newGuard.push_back(N >= 0);
    } else {
        newGuard.push_back(N > 0);
    }
    for (const Expression &ex : rule.getGuard()) {
        newGuard.push_back(ex.subs(updateSubs).subs(N == N-1)); // apply the update N-1 times
    }

    Rule res(RuleLhs(rule.getLhsLoc(), newGuard, iteratedCost), RuleRhs(sink, {}));
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


vector<Rule> BackwardAcceleration::replaceByUpperbounds(const ExprSymbol &N, const Rule &rule) {
    // gather all upper bounds (if possible)
    auto bounds = computeUpperbounds(N, rule.getGuard());

    // avoid rule explosion (by not instantiating N if there are too many bounds)
    if (bounds.empty() || bounds.size() > Config::BackwardAccel::MaxUpperboundsForPropagation) {
        return {rule};
    }

    // create one rule for each upper bound, by instantiating N with this bound
    vector<Rule> res;
    for (const Expression &bound : bounds) {
        GiNaC::exmap subs;
        subs[N] = bound;

        Rule instantiated = rule;
        instantiated.applySubstitution(subs);
        res.push_back(instantiated);
        debugBackwardAccel("instantiation " << subs << " yielded " << instantiated);
    }
    return res;
}

bool BackwardAcceleration::checkCommutation(const std::vector<UpdateMap> &updates) {
    const ExprSymbolSet &relevantVariables = util::RelevantVariables::find(
            rule.getGuard(),
            updates,
            rule.getGuard(),
            varMan);
    for (auto it1 = updates.begin(); it1 < updates.end() - 1; it1++) {
        GiNaC::exmap subs1 = it1->toSubstitution(varMan);
        for (auto it2 = it1 + 1; it2 < updates.end(); it2++) {
            GiNaC::exmap subs2 = it2->toSubstitution(varMan);
            for (const auto &p: subs1) {
                if (relevantVariables.count(Expression(p.first).getAVariable()) > 0) {
                    const Expression &e1 = p.second.subs(subs2).expand();
                    const Expression &e2 = p.first.subs(subs2).subs(subs1).expand();
                    if (e1 != e2) {
                        return false;
                    }
                }
            }
            for (const auto &p: subs2) {
                if (relevantVariables.count(Expression(p.first).getAVariable()) > 0) {
                    const Expression &e1 = p.second.subs(subs1).expand();
                    const Expression &e2 = p.first.subs(subs1).subs(subs2).expand();
                    if (e1 != e2) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

option<Rule> Self::buildInit(unsigned int iterations) const {
    if (iterations <= 1) {
        return {};
    } else {
        GuardList initGuard;
        const GiNaC::exmap &up = rule.getUpdate(0).toSubstitution(varMan);
        GuardList updatedGuard = rule.getGuard();
        Expression updatedCost = rule.getCost();
        Expression initCost = Expression(0);
        for (unsigned int i = 0; i < iterations - 1; i++) {
            initGuard.insert(initGuard.end(), updatedGuard.begin(), updatedGuard.end());
            updatedGuard = updatedGuard.subs(up);
            initCost = initCost + updatedCost;
            updatedCost.applySubs(up);
        }
        UpdateMap initUpdate = rule.getUpdate(0);
        for (unsigned int i = 1; i < iterations; i++) {
            for (auto &p: initUpdate) {
                p.second.applySubs(up);
            }
        }
        return Rule(rule.getLhsLoc(), initGuard, initCost, rule.getRhsLoc(0), initUpdate);
    }
}

Self::AccelerationResult BackwardAcceleration::run() {
    if (!shouldAccelerate()) {
        debugBackwardAccel("won't try to accelerate transition with costs " << rule.getCost());
        return {.res={}, .status=ForwardAcceleration::NotSupported};
    }
    debugBackwardAccel("Trying to accelerate rule " << rule);

    // Remove constraints that are irrelevant for the loop's execution
    std::vector<UpdateMap> updates;
    for (const auto &rhs: rule.getRhss()) {
        updates.push_back(rhs.getUpdate());
    }
    GuardList irrelevantGuard;
    GuardList reducedGuard = MeteringToolbox::reduceGuard(varMan, rule.getGuard(), updates, &irrelevantGuard);

    if (reducedGuard.empty()) {
        return {.res={buildNontermRule()}, .status=ForwardAcceleration::Success};
    }

    if (!checkGuardImplication(reducedGuard, irrelevantGuard)) {
        debugBackwardAccel("Failed to check guard implication");
        Stats::add(Stats::BackwardNonMonotonic);
        return {.res={}, .status=ForwardAcceleration::NonMonotonic};
    }

    // compute the iterated update and cost, with a fresh variable N as iteration step
    ExprSymbol N = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("k"));

    option<Rule> accelerated;
    option<unsigned int> validityBound;
    if (rule.isLinear()) {
        UpdateMap iteratedUpdate = rule.getUpdate(0);
        Expression iteratedCost = rule.getCost();
        GuardList strengthenedGuard = rule.getGuard();
        validityBound = Recurrence::iterateUpdateAndCost(varMan, iteratedUpdate, iteratedCost, strengthenedGuard, N);
        if (!validityBound) {
            debugBackwardAccel("Failed to compute iterated cost/update");
            Stats::add(Stats::BackwardCannotIterate);
            return {.res={}, .status=ForwardAcceleration::NoClosedFrom};
        }

        // compute the resulting rule and try to simplify it by instantiating N (if enabled)
        accelerated = buildAcceleratedLoop(iteratedUpdate, iteratedCost, strengthenedGuard, N, validityBound.get());
    } else {
        if (!checkCommutation(updates)) {
            debugBackwardAccel("Failed to accelerate recursive rule due to non-commutative udpates");
            Stats::add(Stats::BackwardNonCommutative);
            return {.res={}, .status=ForwardAcceleration::NonCommutative};
        }
        option<Recurrence::IteratedUpdates> iteratedUpdates = Recurrence::iterateUpdates(varMan, updates, N);
        if (!iteratedUpdates) {
            debugBackwardAccel("Failed to compute iterated updates");
            Stats::add(Stats::BackwardCannotIterate);
            return {.res={}, .status=ForwardAcceleration::NoClosedFrom};
        }
        validityBound = iteratedUpdates.get().validityBound;
        GuardList strengthenedGuard = rule.getGuard();
        strengthenedGuard.insert(
                strengthenedGuard.end(),
                iteratedUpdates.get().refinement.begin(),
                iteratedUpdates.get().refinement.end());
        unsigned long dimension = rule.getRhss().size();
        const Expression &iteratedCost = reducedGuard.empty() ?
                                         Expression::NontermSymbol :
                                         (pow(dimension, N) - 1) / (dimension - 1);

        // compute the resulting rule and try to simplify it by instantiating N (if enabled)
        accelerated = buildAcceleratedRecursion(iteratedUpdates.get().updates, iteratedCost, strengthenedGuard, N, validityBound.get());
    }
    Stats::add(Stats::BackwardSuccess);
    const option<Rule> init = buildInit(validityBound.get());
    const Rule r = init ? Chaining::chainRules(varMan, init.get(), accelerated.get(), false).get() : accelerated.get();
    if (Config::BackwardAccel::ReplaceTempVarByUpperbounds) {
        return {.res=replaceByUpperbounds(N, r), .status=ForwardAcceleration::Success};
    } else {
        return {{r}, .status=ForwardAcceleration::Success};
    }
}


Self::AccelerationResult BackwardAcceleration::accelerate(VarMan &varMan, const Rule &rule, const LocationIdx &sink) {
    BackwardAcceleration ba(varMan, rule, sink);
    return ba.run();
}
