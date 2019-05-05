#include "backward.hpp"

#include "../debug.hpp"
#include "../z3/z3solver.hpp"
#include "../z3/z3toolbox.hpp"

#include "recurrence/recurrence.hpp"
#include "meter/metertools.hpp"
#include "../expr/guardtoolbox.hpp"
#include "../expr/relation.hpp"
#include "../expr/ginactoz3.hpp"
#include "forward.hpp"

#include <purrs.hh>
#include "../util/stats.hpp"
#include "../util/relevantvariables.hpp"
#include "../analysis/chain.hpp"

using namespace std;

typedef BackwardAcceleration Self;

BackwardAcceleration::BackwardAcceleration(VarMan &varMan, const Rule &rule, const LocationIdx &sink)
        : varMan(varMan), rule(rule), sink(sink) {
    for (const auto &rhs: rule.getRhss()) {
        updates.push_back(rhs.getUpdate());
        updateSubs.push_back(rhs.getUpdate().toSubstitution(varMan));
    }
    nonInvariants = MeteringToolbox::reduceGuard(varMan, rule.getGuard(), updates, &simpleInvariants);
    splitSimpleInvariants();
}

void BackwardAcceleration::splitSimpleInvariants() {
    bool done;
    do {
        done = true;
        auto it = simpleInvariants.begin();
        while (it != simpleInvariants.end()) {
            GuardList updated;
            for (const GiNaC::exmap &up: updateSubs) {
                updated.push_back((*it).subs(up));
            }
            if (Z3Toolbox::isValidImplication(simpleInvariants, updated)) {
                it++;
            } else {
                simpleInvariants.erase(it);
                conditionalInvariants.push_back(*it);
                done = false;
                break;
            }
        }
    } while (!done);
}

bool BackwardAcceleration::shouldAccelerate() const {
    return !rule.getCost().isNontermSymbol() && rule.getCost().isPolynomial();
}

bool BackwardAcceleration::checkGuardImplication() const {
    Z3Context context;
    Z3Solver solver(context);
    vector<z3::expr> lhss, rhss;

    for (const Expression &ex : simpleInvariants) {
        solver.add(GinacToZ3::convert(ex, context));
    }

    for (const auto &update: updateSubs) {
        solver.push();
        for (const Expression &ex : nonInvariants) {
            solver.add(GinacToZ3::convert(ex.subs(update), context));
            rhss.push_back(GinacToZ3::convert(ex, context));
        }
        debugBackwardAccel("Checking guard implication:  " << lhs << "  ==>  " << rhs);
        if (solver.check() != z3::sat) {
            return false;
        }
        solver.add(!Z3Toolbox::concat(context, rhss, Z3Toolbox::ConcatAnd));
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
                                                const GuardList &strengthenedGuard,
                                                const ExprSymbol &N) const
{
    GiNaC::exmap updateSubs = iteratedUpdate.toSubstitution(varMan);
    GuardList newGuard = strengthenedGuard;
    newGuard.push_back(N >= 0);
    for (const Expression &ex : nonInvariants) {
        newGuard.erase(std::find(newGuard.begin(), newGuard.end(), ex));
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
    if (iterations == 0) {
        return {};
    } else {
        Rule res = rule;
        for (unsigned int i = 0; i < iterations - 1; i++) {
            Chaining::chainRules(varMan, res, rule, false);
        }
        return res;
    }
}

Self::AccelerationResult BackwardAcceleration::run() {
    if (!shouldAccelerate()) {
        debugBackwardAccel("won't try to accelerate transition with costs " << rule.getCost());
        return {.res={}, .status=ForwardAcceleration::NotSupported};
    }
    debugBackwardAccel("Trying to accelerate rule " << rule);

    if (nonInvariants.empty() && Z3Toolbox::isValidImplication(rule.getGuard(), {rule.getCost() > 0})) {
        return {.res={buildNontermRule()}, .status=ForwardAcceleration::Success};
    }

    if (!checkGuardImplication()) {
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
        accelerated = buildAcceleratedLoop(iteratedUpdate, iteratedCost, strengthenedGuard, N);
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
        const Expression &iteratedCost = nonInvariants.empty() ?
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
