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


bool Accelerator::nestRules(const Complexity &currentCpx, const InnerCandidate &inner, const OuterCandidate &outer) {
    bool res = false;
    const LinearRule innerRule = its.getLinearRule(inner.newRule);
    const LinearRule outerRule = its.getLinearRule(outer.oldRule);

    // Avoid nesting a loop with its original transition or itself
    if (inner.oldRule == outer.oldRule || inner.newRule == outer.oldRule) {
        return false;
    }

    // Check by some heuristic if it makes sense to nest inner and outer
    if (!canNest(innerRule, outerRule)) {
        return false;
    }

    // Lambda that performs the nesting/acceleration.
    // If successful, also tries to chain the second rule in front of the accelerated (nested) rule.
    // This can improve results, since we do not know which loop is executed first.
    auto nestAndChain = [&](const LinearRule &first, const LinearRule &second) -> bool {
        auto optNested = Chaining::chainRules(its, first, second);
        if (optNested) {
            LinearRule nestedRule = optNested.get();

            // Simplify the rule again (chaining can introduce many useless constraints)
            Preprocess::simplifyRule(its, nestedRule);

            // Note that we do not try all heuristics or backward accel to keep nesting efficient
            const std::vector<Rule> &accelRules = Backward::accelerate(its, nestedRule, sinkLoc).res;
            bool success = false;
            for (const Rule &accelRule: accelRules) {
                Complexity newCpx = AsymptoticBound::determineComplexityViaSMT(
                        its,
                        accelRule.getGuard(),
                        accelRule.getCost(),
                        false,
                        currentCpx).cpx;
                if (newCpx > currentCpx) {
                    addNestedRule(accelRule, second, inner.newRule, outer.oldRule);
                    success = true;
                }
            }
            return success;
        }
        return false;
    };

    // Try to nest, executing inner loop first (and try to chain outerRule in front)
    res |= nestAndChain(innerRule, outerRule);

    // Try to nest, executing outer loop first (and try to chain innerRule in front).
    // If the previous nesting was already successful, it probably covers most execution
    // paths already, since "nestAndChain" also tries to chain the second rule in front.
    if (!res) {
        res |= nestAndChain(outerRule, innerRule);
    }

    return res;
}


void Accelerator::performNesting(vector<InnerCandidate> inner, vector<OuterCandidate> outer) {
    debugAccel("Nesting with " << inner.size() << " inner and " << outer.size() << " outer candidates");
    bool changed = false;

    // Try to combine previously identified inner and outer candidates via chaining,
    // then try to accelerate the resulting rule
    for (const InnerCandidate &in : inner) {
        Rule r = its.getLinearRule(in.newRule);
        Complexity cpx = AsymptoticBound::determineComplexityViaSMT(
                its,
                r.getGuard(),
                r.getCost(),
                false,
                Complexity::Const).cpx;
        for (const OuterCandidate &out : outer) {
            changed |= nestRules(cpx, in, out);
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

const Rule Accelerator::chain(const Rule &rule) const {
    if (!rule.isLinear()) return rule;
    Rule res = rule;
    for (const auto &p: rule.getUpdate(0)) {
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
    const Rule &orig = res;
    unsigned int last = numNotInUpdate(res.getUpdate(0));
    do {
        Rule chained = Chaining::chainRules(its, res, orig, false).get();
        unsigned int next = numNotInUpdate(chained.getUpdate(0));
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

const Forward::Result Accelerator::strengthenAndAccelerate(const Rule &rule) const {
    option<Forward::ResultKind> status;
    std::vector<Forward::MeteredRule> rules;
    stack<Rule> todo;
    todo.push(chain(rule));
    bool unrestricted = true;
    bool unrestrictedNonTerm = false;
    do {
        Rule r = todo.top();
        bool sat = Z3Toolbox::checkAll({r.getGuard()}) == z3::sat;
        if (sat && r.getCost().isNontermSymbol()) {
            todo.pop();
            if (!status) {
                status = Forward::Success;
                unrestrictedNonTerm = true;
            }
            continue;
        } else if (!sat) {
            todo.pop();
            if (!status) {
                status = Forward::NonMonotonic;
                unrestricted = false;
            }
            continue;
        }
        // first try to prove non-termination
        option<std::pair<Rule, Forward::ResultKind>> p = nonterm::NonTerm::apply(r, its, sinkLoc);
        if (p) {
            rules.emplace_back("non-termination", p.get().first);
            if (unrestricted && p.get().second == Forward::Success) {
                unrestrictedNonTerm = true;
            }
        }
        todo.pop();
        if (!unrestrictedNonTerm) {
            BackwardAcceleration::AccelerationResult res = Backward::accelerate(its, r, sinkLoc);
            // if backwards acceleration is not supported, we can only hope to prove non-termination
            // for proving non-termination, only invariance is of interest
            std::vector<strengthening::Mode> strengtheningModes = strengthening::Modes::modes();
            // store the result for the original rule so that we can return it if we fail
            if (!status) {
                status = res.status;
                if (status.get() != Forward::Success) {
                    unrestricted = false;
                }
            }
            if (res.res.empty()) {
                vector<Rule> strengthened = strengthening::Strengthener::apply(r, its, strengtheningModes);
                for (const Rule &sr: strengthened) {
                    debugBackwardAccel("invariant inference yields " << sr);
                    todo.push(sr);
                }
            } else {
                for (const Rule &ar: res.res) {
                    rules.emplace_back("backward acceleration", ar);
                }
            }
        }
    } while (!todo.empty());
    if (!rules.empty()) {
        status = unrestrictedNonTerm || unrestricted ? Forward::Success : Forward::SuccessWithRestriction;
    }
    return {.result=status.get(), .rules=rules};
}

Forward::Result Accelerator::tryAccelerate(const Rule &rule) const {
    // Forward acceleration
    assert(Config::Accel::UseForwardAccel || Config::Accel::UseBackwardAccel);
    Forward::Result res;
    if (Config::Accel::UseForwardAccel) {
        return Forward::accelerate(its, rule, sinkLoc);
    }

    // Try backward acceleration only if forward acceleration failed,
    // or if it only succeeded by restricting the guard. In this case,
    // we keep the rules from forward and just add the ones from backward acceleration.
    if (Config::Accel::UseBackwardAccel) {
        return strengthenAndAccelerate(rule);
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
    vector<InnerCandidate> innerCandidates;
    vector<OuterCandidate> outerCandidates;

    // Try to accelerate all loops
    for (TransIdx loop : loops) {
        if (Timeout::soft()) return;

        // Forward and backward accelerate (and partial deletion for nonlinear rules)
        Forward::Result res = accelerateOrShorten(its.getRule(loop));

        // Interpret the results, add new rules
        switch (res.result) {
            case Forward::NotSupported:
                keepRules.insert(loop);
                proofout << "Acceleration of " << loop << " is not supported." << endl;
                break;

            case Forward::TooComplicated:
                // rules is probably not relevant for nesting
                keepRules.insert(loop);
                proofout << "Found no metering function for rule " << loop << " (rule is too complicated)." << endl;
                break;

            case Forward::NoMetering:
                if (its.getRule(loop).isLinear()) {
                    outerCandidates.push_back({loop});
                }
                keepRules.insert(loop);
                proofout << "Found no metering function for rule " << loop << "." << endl;
                break;

            case Forward::NoClosedFrom:
                if (its.getRule(loop).isLinear()) {
                    outerCandidates.push_back({loop});
                }
                keepRules.insert(loop);
                proofout << "Found no closed form for " << loop << "." << endl;
                break;

            case Forward::NonCommutative:
                throw std::logic_error("Acceleration failed due to non-commutative updates, but rule should have been shortened.");

            case Forward::NonMonotonic:
                if (its.getRule(loop).isLinear()) {
                    innerCandidates.push_back(InnerCandidate{.oldRule=loop,.newRule=loop});
                    outerCandidates.push_back({loop});
                }
                keepRules.insert(loop);
                proofout << "Failed to prove monotonicity of the guard of rule " << loop << "." << endl;
                break;


            case Forward::SuccessWithRestriction:
                // If we only succeed by extending the rule's guard, we make the rule more restrictive
                // and can thus lose execution paths. So we also keep the original, unaccelerated rule.
                keepRules.insert(loop);

                // fall-through

            case Forward::Success:
            {
                bool isNonterm = false;

                // Add accelerated rules, also mark them as inner nesting candidates
                for (Forward::MeteredRule accel : res.rules) {
                    TransIdx added = addResultingRule(accel.rule);
                    proofout << "Accelerated rule " << loop << " with " << accel.info;
                    proofout << ", yielding the new rule " << added << "." << endl;

                    if (accel.rule.isSimpleLoop()) {
                        // accel.rule is a simple loop iff the original was linear and not non-terminating.
                        innerCandidates.push_back(InnerCandidate{.oldRule=loop,.newRule=added});
                    }

                    isNonterm = isNonterm || accel.rule.getCost().isNontermSymbol();
                }

                // If the guard was modified, the original rule might not be non-terminating
                if (res.result == Forward::SuccessWithRestriction) {
                    isNonterm = false;
                }

                // The original rule could still be an outer loop for nesting,
                // unless it is non-terminating (so nesting will not improve the result).
                if (its.getRule(loop).isLinear() && !isNonterm) {
                    outerCandidates.push_back({loop});
                }
                break;
            }
        }
    }

    // Nesting
    if (Config::Accel::TryNesting) {
        performNesting(std::move(innerCandidates), std::move(outerCandidates));
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
