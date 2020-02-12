/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#include "accelerator.hpp"

#include "../analysis/preprocess.hpp"
#include "recurrence/recurrence.hpp"
#include "meter/metering.hpp"
#include "../z3/z3toolbox.hpp"

#include "../its/rule.hpp"
#include "../its/export.hpp"

#include "../analysis/chain.hpp"
#include "../analysis/prune.hpp"

#include "../debug.hpp"
#include "../util/stats.hpp"
#include "../util/timing.hpp"
#include "../util/timeout.hpp"
#include "forward.hpp"
#include "backward.hpp"

#include <queue>
#include "../asymptotic/asymptoticbound.hpp"
#include "../strengthening/strengthener.hpp"
#include <stdexcept>
#include "../nonterm/nonterm.hpp"


using namespace std;
namespace Forward = ForwardAcceleration;
using Backward = BackwardAcceleration;


Accelerator::Accelerator(ITSProblem &its, LocationIdx loc, std::set<TransIdx> &resultingRules)
    : its(its), targetLoc(loc), resultingRules(resultingRules)
{
    // We need a sink location for INF rules and nonlinear rules
    // To avoid too many parallel rules (which would then be pruned), we use a new one for each run
    sinkLoc = its.addLocation();
}


TransIdx Accelerator::addResultingRule(Rule rule) {
    TransIdx idx = its.addRule(rule);
    resultingRules.insert(idx);
    return idx;
}


// #####################
// ##  Preprocessing  ##
// #####################

bool Accelerator::simplifySimpleLoops() {
    bool res = false;
    vector<TransIdx> loops = its.getSimpleLoopsAt(targetLoc);

    // Simplify all all simple loops.
    // This is especially useful to eliminate temporary variables before metering.
    if (Config::Accel::SimplifyRulesBefore) {
        for (TransIdx loop : loops) {
            res |= Preprocess::simplifyRule(its, its.getRuleMut(loop));
            if (Timeout::soft()) return res;
        }
    }

    // Remove duplicate rules (does not happen frequently, but the syntactical check should be cheap anyway)
    if (Pruning::removeDuplicateRules(its, loops)) {
        res = true;
        debugAccel("Removed some duplicate simple loops");
    }

    return res;
}


// ########################
// ##  Nesting of Loops  ##
// ########################

bool Accelerator::canNest(const LinearRule &inner, const LinearRule &outer) const {
    // Collect all variables appearing in the inner guard
    ExprSymbolSet innerGuardSyms;
    for (const Expression &ex : inner.getGuard()) {
        ex.collectVariables(innerGuardSyms);
    }

    // If any of these variables is affected by the outer update,
    // then applying the outer loop can affect the inner loop's condition,
    // so it might be possible to execute the inner loop again (and thus nesting might work).
    for (const auto &it : outer.getUpdate()) {
        ExprSymbol updated = its.getVarSymbol(it.first);
        if (innerGuardSyms.count(updated) > 0) {
            return true;
        }
    }

    return false;
}


void Accelerator::addNestedRule(const Rule &metered, const LinearRule &chain,
                                TransIdx inner, TransIdx outer)
{
    // Add the new rule
    TransIdx added = addResultingRule(metered);

    // The outer rule was accelerated (after nesting), so we do not need to keep it anymore
    keepRules.erase(outer);

    proofout << "Nested simple loops " << outer << " (outer loop) and " << inner;
    proofout << " (inner loop) with " << metered;
    proofout << ", resulting in the new rules: " << added;

    // Try to combine chain and the accelerated loop
    auto chained = Chaining::chainRules(its, chain, metered);
    if (chained) {
        TransIdx added = addResultingRule(chained.get());
        proofout << ", " << added;
    }
    proofout << "." << endl;
}


bool Accelerator::nestRules(const NestingCandidate &fst, const NestingCandidate &snd) {
    // Avoid nesting two new rules
    if (fst.oldRule != fst.newRule && snd.oldRule != snd.newRule) {
        return false;
    }

    // Avoid nesting a loop with its original transition or itself
    if (fst.oldRule == snd.oldRule || fst.newRule == snd.newRule) {
        return false;
    }

    const LinearRule first = its.getLinearRule(fst.newRule);
    const LinearRule second = its.getLinearRule(snd.newRule);

    // Check by some heuristic if it makes sense to nest inner and outer
    if (!canNest(first, second)) {
        return false;
    }

    auto optNested = Chaining::chainRules(its, first, second);
    if (optNested) {
        LinearRule nestedRule = optNested.get();

        // Simplify the rule again (chaining can introduce many useless constraints)
        Preprocess::simplifyRule(its, nestedRule);

        // Note that we do not try all heuristics or backward accel to keep nesting efficient
        const std::vector<Rule> &accelRules = Backward::accelerate(its, nestedRule, sinkLoc).res;
        bool success = false;
        Complexity currentCpx = fst.cpx > snd.cpx ? fst.cpx : snd.cpx;
        for (const Rule &accelRule: accelRules) {
            Complexity newCpx = AsymptoticBound::determineComplexityViaSMT(
                        its,
                        accelRule.getGuard(),
                        accelRule.getCost(),
                        false,
                        currentCpx).cpx;
            if (newCpx > currentCpx) {
                addNestedRule(accelRule, second, fst.newRule, snd.oldRule);
                success = true;
            }
        }
        return success;
    }
    return false;
}


void Accelerator::performNesting(vector<NestingCandidate> candidates) {
    debugAccel("Nesting with " << inner.size() << " inner and " << outer.size() << " outer candidates");

    // Try to combine previously identified inner and outer candidates via chaining,
    // then try to accelerate the resulting rule
    for (const NestingCandidate &in : candidates) {
        Rule r = its.getLinearRule(in.newRule);
        for (const NestingCandidate &out : candidates) {
            nestRules(in, out);
            if (Timeout::soft()) return;
        }
    }
}


// ############################
// ## Removal (cleaning up)  ##
// ############################

void Accelerator::removeOldLoops(const vector<TransIdx> &loops) {
    // Remove all old loops, unless we have decided to keep them
    bool foundOne = true;
    for (TransIdx loop : loops) {
        if (keepRules.count(loop) == 0) {
            if (foundOne) {
                proofout << "Removing the simple loops:";
                foundOne = false;
            }
            proofout << " " << loop;
            its.removeRule(loop);
        }
    }
    if (!foundOne) {
        proofout << "." << endl;
    }

    // In some cases, two loops can yield similar accelerated rules, so we prune duplicates
    // and have to remove rules that were removed from resultingRules.
    if (Pruning::removeDuplicateRules(its, resultingRules)) {
        proofout << "Also removing duplicate rules:";
        auto it = resultingRules.begin();
        while (it != resultingRules.end()) {
            if (its.hasRule(*it)) {
                it++;
            } else {
                proofout << " " << (*it);
                it = resultingRules.erase(it);
            }
        }
        proofout << "." << endl;
    }
}

const LinearRule Accelerator::chain(const LinearRule &rule) const {
    LinearRule res = rule;
    for (const auto &p: rule.getUpdate()) {
        const ExprSymbol &var = its.getVarSymbol(p.first);
        const Expression &up = p.second.expand();
        const ExprSymbolSet &upVars = up.getVariables();
        if (upVars.find(var) != upVars.end()) {
            if (up.isPolynomial() && up.degree(var) == 1) {
                const Expression &coeff = up.coeff(var);
                if (coeff.isRationalConstant() && coeff < 0) {
                    res = Chaining::chainRules(its, res, res, false).get();
                    break;
                }
            }
        }
    }
    const LinearRule &orig = res;
    unsigned int last = numNotInUpdate(res.getUpdate());
    do {
        LinearRule chained = Chaining::chainRules(its, res, orig, false).get();
        unsigned int next = numNotInUpdate(chained.getUpdate());
        if (next != last) {
            last = next;
            res = chained;
            continue;
        }
    } while (false);
    return res;
}

unsigned int Accelerator::numNotInUpdate(const UpdateMap &up) const {
    unsigned int res = 0;
    for (auto const &p: up) {
        const ExprSymbol &x = its.getVarSymbol(p.first);
        const ExprSymbolSet &vars = p.second.getVariables();
        if (!vars.empty() && vars.find(x) == vars.end()) {
            res++;
        }
    }
    return res;
}

// ###################
// ## Acceleration  ##
// ###################

const Forward::Result Accelerator::strengthenAndAccelerate(const LinearRule &rule) const {
    option<Forward::ResultKind> status;
    std::vector<Forward::MeteredRule> rules;
    stack<LinearRule> todo;
    // chain rule if necessary
    const LinearRule &r = chain(rule);
    bool sat = Z3Toolbox::checkAll({r.getGuard()}) == z3::sat;
    // only proceed if the guard is sat
    if (sat) {
        // first try to prove nonterm
        option<std::pair<Rule, Forward::ResultKind>> p = nonterm::NonTerm::apply(r, its, sinkLoc);
        if (p) {
            rules.emplace_back("non-termination", p.get().first);
            // if the rule is universaly non-termianting, we are done
            if (p.get().second == Forward::Success) {
                return {.result=Forward::Success, .rules=rules};
            }
        }
        // try acceleration
        BackwardAcceleration::AccelerationResult res = Backward::accelerate(its, r, sinkLoc);
        if (res.res.empty()) {
            // if acceleration failed, the best we can hope for is a restricted result
            status = Forward::SuccessWithRestriction;
        } else {
            // if acceleration succeeded, save the result, but we still try to find cases where the rule does not terminate
            status = res.status;
            for (const Rule &ar: res.res) {
                rules.emplace_back("acceleration calculus", ar);
            }
        }
        // strengthen in order to prove non-termination
        vector<LinearRule> strengthened = strengthening::Strengthener::apply(r, its, strengthening::Modes::modes());
        for (const LinearRule &sr: strengthened) {
            debugBackwardAccel("invariant inference yields " << sr);
            todo.push(sr);
        }
        // alter between non-termination proving and strengthening
        while (!todo.empty()) {
            LinearRule r = todo.top();
            bool sat = Z3Toolbox::checkAll({r.getGuard()}) == z3::sat;
            // only proceed if the guard is sat
            if (sat) {
                // try to prove non-termination
                option<std::pair<Rule, Forward::ResultKind>> p = nonterm::NonTerm::apply(r, its, sinkLoc);
                if (p) {
                    rules.emplace_back("non-termination", p.get().first);
                    // if the rule is universally non-terminating, we are done
                    if (p->second == Forward::Success) {
                        todo.pop();
                        continue;
                    }
                }
                // strengthen
                vector<LinearRule> strengthened = strengthening::Strengthener::apply(r, its, strengthening::Modes::modes());
                todo.pop();
                for (const LinearRule &sr: strengthened) {
                    debugBackwardAccel("invariant inference yields " << sr);
                    todo.push(sr);
                }
            } else {
                todo.pop();
            }
        }
    }
    if (rules.empty()) {
        status = Forward::NonMonotonic;
    }
    return {.result=status.get(), .rules=rules};
}

Forward::Result Accelerator::tryAccelerate(const Rule &rule) const {
    // Forward acceleration
    if (!rule.isLinear()) {
        return Forward::accelerate(its, rule, sinkLoc);
    } else {
        assert(Config::Accel::UseForwardAccel || Config::Accel::UseBackwardAccel);
        Forward::Result res;
        if (Config::Accel::UseForwardAccel) {
            return Forward::accelerate(its, rule, sinkLoc);
        }

        if (Config::Accel::UseBackwardAccel) {
            return strengthenAndAccelerate(rule.toLinear());
        }
    }

}


Forward::Result Accelerator::accelerateOrShorten(const Rule &rule) const {
    using namespace Forward;

    // Accelerate the original rule
    auto res = tryAccelerate(rule);

    // Stop if heuristic is not applicable or acceleration was already successful
    if (!Config::Accel::PartialDeletionHeuristic || rule.isLinear()) {
        return res;
    }
    if (res.result == Success || res.result == SuccessWithRestriction) {
        return res;
    }

    // Remember the original result (we return this in case shortening fails)
    auto originalRes = res;

    // Helper lambda that calls tryAccelerate and adds to the proof output.
    // Returns true if accelerating was successful.
    auto tryAccel = [&](const Rule &newRule) {
        res = tryAccelerate(newRule);
        if (res.result == Success || res.result == SuccessWithRestriction) {
            for (Forward::MeteredRule &rule : res.rules) {
                rule = rule.appendInfo(" (after partial deletion)");
            }
            return true;
        }
        return false;
    };

    // If metering failed, we remove rhss to ease metering.
    // To keep the code efficient, we only try all pairs and each single rhs.
    // We start with pairs of rhss, since this can still yield exponential complexity.
    const vector<RuleRhs> &rhss = rule.getRhss();

    for (unsigned int i=0; i < rhss.size(); ++i) {
        for (unsigned int j=i+1; j < rhss.size(); ++j) {
            vector<RuleRhs> newRhss{ rhss[i], rhss[j] };
            Rule newRule(rule.getLhs(), newRhss);
            if (tryAccel(newRule)) {
                debugAccel("Success after shortening rule to 2 rhss");
                return res;
            }
        }
    }

    for (RuleRhs rhs : rhss) {
        if (tryAccel(Rule(rule.getLhs(), rhs))) {
            debugAccel("Success after shortening rule to 1 rhs");
            return res;
        }
    }

    return originalRes;
}


// #####################
// ## Main algorithm  ##
// #####################

void Accelerator::run() {
    // Simplifying rules might make it easier to find metering functions
    if (simplifySimpleLoops()) {
        proofout << "Simplified some of the simple loops (and removed duplicate rules)." << endl;
    }

    // Since we might add accelerated loops, we store the list of loops before acceleration
    vector<TransIdx> loops = its.getSimpleLoopsAt(targetLoc);
    if (loops.empty()) {
        proofout << "No simple loops left to accelerate." << endl;
        return; // may happen if rules get removed in simplifySimpleLoops
    }

    // Proof output
    proofout << "Accelerating the following rules:" << endl;
    for (TransIdx loop : loops) {
        ITSExport::printLabeledRule(loop, its, proofout);
    }
    proofout << endl;

    // While accelerating, collect rules that might be feasible for nesting
    // Inner candidates are accelerated rules, since they correspond to a loop within another loop.
    // Outer candidates are loops that cannot be accelerated on their own (because they are missing their inner loop)
    vector<NestingCandidate> nestingCandidates;

    // Try to accelerate all loops
    for (TransIdx loop : loops) {
        if (Timeout::soft()) return;

        // Forward and backward accelerate (and partial deletion for nonlinear rules)
        const Rule &r = its.getRule(loop);
        Forward::Result res = accelerateOrShorten(r);

        if (its.getRule(loop).isLinear()) {
            Complexity cpx = AsymptoticBound::determineComplexityViaSMT(
                    its,
                    r.getGuard(),
                    r.getCost(),
                    false,
                    Complexity::Const).cpx;
            nestingCandidates.push_back({loop, loop, cpx});
        }

        if (res.result != Forward::Success) {
            keepRules.insert(loop);
        }

        // Interpret the results, add new rules
        switch (res.result) {
            case Forward::NotSupported:
                proofout << "Acceleration of " << loop << " is not supported." << endl;
                break;

            case Forward::TooComplicated:
                proofout << "Found no metering function for rule " << loop << " (rule is too complicated)." << endl;
                break;

            case Forward::NoMetering:
                proofout << "Found no metering function for rule " << loop << "." << endl;
                break;

            case Forward::NoClosedFrom:
                proofout << "Found no closed form for " << loop << "." << endl;
                break;

            case Forward::NonMonotonic:
                proofout << "Failed to prove monotonicity of the guard of rule " << loop << "." << endl;
                break;

            case Forward::SuccessWithRestriction:
            case Forward::Success:
            {
                // Add accelerated rules, also mark them as inner nesting candidates
                for (Forward::MeteredRule accel : res.rules) {
                    TransIdx added = addResultingRule(accel.rule);
                    proofout << "Accelerated rule " << loop << " with " << accel.info;
                    proofout << ", yielding the new rule " << added << "." << endl;

                    if (accel.rule.isSimpleLoop()) {
                        // accel.rule is a simple loop iff the original was linear and not non-terminating.
                        Complexity cpx = AsymptoticBound::determineComplexityViaSMT(
                                its,
                                accel.rule.getGuard(),
                                accel.rule.getCost(),
                                false,
                                Complexity::Const).cpx;
                        nestingCandidates.push_back(NestingCandidate{.oldRule=loop, .newRule=added, .cpx=cpx});
                    }
                }
                break;
            }
        }
    }

    // Nesting
    if (Config::Accel::TryNesting) {
        performNesting(std::move(nestingCandidates));
        if (Timeout::soft()) return;
    }

    // Chaining accelerated rules amongst themselves would be another heuristic.
    // Although it helps in certain examples, it is currently not implemented
    // as it would most probably result in too many rules (and would thus be expensive).
    // It can easily be added in this place in the future, if desired.

    // Simplify the guards of accelerated rules.
    // Especially backward acceleration and nesting can introduce superfluous constraints.
    for (TransIdx rule : resultingRules) {
        Preprocess::simplifyGuard(its.getRuleMut(rule).getGuardMut());
    }

    // Keep rules for which acceleration failed (maybe these rules are in fact not loops).
    // We add them to resultingRules so they are chained just like accelerated rules.
    for (TransIdx rule : keepRules) {
        resultingRules.insert(rule);
    }

    // Remove old rules
    removeOldLoops(loops);

    // Remove sink location if we did not need it
    if (!its.hasTransitionsTo(sinkLoc)) {
        its.removeOnlyLocation(sinkLoc);
    }
}


// #######################
// ## Public interface  ##
// #######################

bool Accelerator::accelerateSimpleLoops(ITSProblem &its, LocationIdx loc, std::set<TransIdx> &resultingRules) {
    if (its.getSimpleLoopsAt(loc).empty()) {
        return false;
    }

    proofout << endl;
    proofout.setLineStyle(ProofOutput::Headline);
    proofout << "Accelerating simple loops of location " << loc << "." << endl;
    proofout.increaseIndention();
    bool wasEnabled = proofout.setEnabled(Config::Output::ProofAccel);

    // Accelerate all loops (includes optimizations like nesting)
    Accelerator accel(its, loc, resultingRules);
    accel.run();

    proofout.setEnabled(wasEnabled);
    proofout.decreaseIndention();
    return true;
}
