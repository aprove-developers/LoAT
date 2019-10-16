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
#include "accelerationproblem.hpp"

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

            std::vector<LinearRule> accelRules;
            option<std::pair<LinearRule, ForwardAcceleration::ResultKind>> nontermRes = nonterm::NonTerm::apply(nestedRule, its, sinkLoc);
            if (nontermRes) {
                accelRules.push_back(nontermRes.get().first);
            }
            if (!nontermRes || nontermRes.get().second != ForwardAcceleration::Success) {
                if (Config::Accel::UseBackwardAccel) {
                    auto rules = Backward::accelerate(its, nestedRule, sinkLoc).res;
                    accelRules.insert(accelRules.end(), rules.begin(), rules.end());
                } else {
                    assert(Config::Accel::UseForwardAccel);
                    auto rule = Forward::accelerateFast(its, nestedRule, sinkLoc);
                    if (rule) {
                        accelRules.push_back(rule.get().rule.toLinear());
                    }
                }
            }
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
                foundOne = false;
            }
            its.removeRule(loop);
        }
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
    const Rule &orig = res;
    unsigned int last = numNotInUpdate(res.getUpdate());
    do {
        LinearRule chained = Chaining::chainRules(its, res, orig, false).get().toLinear();
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
    todo.push(rule);
    bool unrestricted = true;
    bool unrestrictedNonTerm = false;
    do {
        LinearRule r = todo.top();
//        bool sat = Z3Toolbox::checkAll(r.getGuard()) == z3::sat;
//        if (sat && r.getCost().isNontermSymbol()) {
//            todo.pop();
//            if (!status) {
//                status = Forward::Success;
//                unrestrictedNonTerm = true;
//            }
//            continue;
//        } else if (!sat) {
//            todo.pop();
//            if (!status) {
//                status = Forward::NonMonotonic;
//                unrestricted = false;
//            }
//            continue;
//        }
        // first try to prove non-termination
//        option<std::pair<LinearRule, Forward::ResultKind>> p = nonterm::NonTerm::apply(r, its, sinkLoc);
//        if (p) {
//            rules.emplace_back("non-termination", p.get().first);
//            if (unrestricted && p.get().second == Forward::Success) {
//                unrestrictedNonTerm = true;
//            }
//        }
        todo.pop();
        if (!unrestrictedNonTerm) {
            BackwardAcceleration::AccelerationResult res = Backward::accelerate(its, r, sinkLoc);
            // if backwards acceleration is not supported, we can only hope to prove non-termination
            // for proving non-termination, only invariance is of interest
            // store the result for the original rule so that we can return it if we fail
            if (!status) {
                status = res.status;
                if (status.get() != Forward::Success) {
                    unrestricted = false;
                }
            }
            if (res.res.empty()) {
                if (Config::BackwardAccel::Strengthen) {
                    vector<LinearRule> strengthened = strengthening::Strengthener::apply(r, its);
                    for (const LinearRule &sr: strengthened) {
                        debugBackwardAccel("invariant inference yields " << sr);
                        todo.push(sr);
                    }
                }
            } else {
                for (const LinearRule &ar: res.res) {
                    rules.emplace_back("backward acceleration", ar);
                }
            }
        }
    } while (!todo.empty());
    if (!rules.empty()) {
        status = unrestrictedNonTerm || unrestricted ? Forward::Success : Forward::SuccessWithRestriction;
    }
    return {status.get(), rules};
}

Forward::Result Accelerator::tryAccelerate(const Rule &rule) const {
//    return Forward::accelerate(its, rule, sinkLoc);
//    return strengthenAndAccelerate(chain(rule.toLinear()));
    option<AccelerationProblem> ap = AccelerationCalculus::init(rule.toLinear(), its);
    if (!ap) {
        return {Forward::NoClosedFrom, {}};
    }
    option<AccelerationProblem> res = AccelerationCalculus::solve(ap.get());
    if (!res) {
        return {Forward::NonMonotonic, {}};
    } else {
        UpdateMap up;
        for (const auto &e: res.get().closed) {
            up[its.getVarIdx(Expression(e.first).getAVariable())] = e.second;
        }
        Forward::Result result;
        LinearRule accelerated(rule.getLhs().getLoc(), res.get().res, Expression(1), rule.getLhs().getLoc(), up);
        result.rules.push_back({"acceleration calculus", accelerated});
        if (res.get().equivalent) {
            result.result = Forward::Success;
        } else {
            result.result = Forward::SuccessWithRestriction;
        }
        return result;
    }
}


Forward::Result Accelerator::accelerateOrShorten(const Rule &rule) const {
    using namespace Forward;

    // Accelerate the original rule
    return tryAccelerate(rule);
}


// #####################
// ## Main algorithm  ##
// #####################

bool Accelerator::run() {
    // Simplifying rules might make it easier to find metering functions
    if (simplifySimpleLoops()) {
        proofout << "Simplified some of the simple loops (and removed duplicate rules)." << endl;
    }

    // Since we might add accelerated loops, we store the list of loops before acceleration
    vector<TransIdx> loops = its.getSimpleLoopsAt(targetLoc);
    if (loops.empty()) {
        proofout << "No simple loops left to accelerate." << endl;
        return false; // may happen if rules get removed in simplifySimpleLoops
    }

    // Proof output
    proofout << "Accelerating the following rules:" << endl;
    for (TransIdx loop : loops) {
        ITSExport::printLabeledRule(loop, its, proofout);
    }
    proofout << endl;

    // Try to accelerate all loops
    bool changed = false;
    for (TransIdx loop : loops) {
        if (Timeout::soft()) return changed;

        // Forward and backward accelerate (and partial deletion for nonlinear rules)
        Forward::Result res = accelerateOrShorten(its.getRule(loop));

        // Interpret the results, add new rules
        switch (res.result) {
            case Forward::Success:
            {
                changed = true;
                // Add accelerated rules, also mark them as inner nesting candidates
                std::vector<TransIdx> added;
                std::string info;
                for (Forward::MeteredRule accel : res.rules) {
                    info = accel.info;
                    added.push_back(addResultingRule(accel.rule));
                }
                proofout.section("Success");
                proofout << endl << "Equivalently accelerated rule " << loop << " with " << info;
                proofout << ", yielding the new rules ";
                for (const auto &e: added) {
                    proofout << " " << e;
                }
                proofout << endl;
                break;
            }
            case Forward::SuccessWithRestriction:
            {
                changed = true;
                // Add accelerated rules, also mark them as inner nesting candidates
                std::vector<TransIdx> added;
                std::string info;
                for (Forward::MeteredRule accel : res.rules) {
                    info = accel.info;
                    added.push_back(addResultingRule(accel.rule));
                }
                proofout.section("Success");
                proofout << endl << "Approximately accelerated rule " << loop << " with " << info;
                proofout << ", yielding the new rules ";
                for (const auto &e: added) {
                    proofout << " " << e;
                }
                proofout << endl;
                break;
            }
            default:
            {
                proofout.section("Failed");
                proofout << endl << "Failed to accelerate loop." << endl;
            }
        }
        proofout << std::endl;
    }

    // Simplify the guards of accelerated rules.
    for (TransIdx rule : resultingRules) {
        Preprocess::simplifyGuard(its.getRuleMut(rule).getGuardMut());
    }

    // Remove old rules
    removeOldLoops(loops);

    return changed;
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
    bool res = accel.run();

    proofout.setEnabled(wasEnabled);
    proofout.decreaseIndention();
    return res;
}
