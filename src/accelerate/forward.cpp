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

#include "forward.h"

#include "recurrence/recurrence.h"
#include "meter/metering.h"

#include "global.h"
#include "debug.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"


using namespace std;
using ForwardAcceleration::Result;
using ForwardAcceleration::MeteredRule;


/**
 * Helper function that searches for a metering function and, if successful,
 * tries to compute the iterated cost and update (for linear rules) or tries
 * to approximate the iterated cost (for nonlinear rules).
 *
 * @param sink Used for non-terminating and nonlinear rules (since we do not know to what they evaluate).
 * @param conflictVar If the metering result is ConflictVar, conflictVar is set to the conflicting variables,
 * otherwise it is not modified.
 */
static Result meterAndIterate(VarMan &varMan, Rule rule, LocationIdx sink, option<VariablePair> &conflictVar) {
    using namespace ForwardAcceleration;
    Result res;

    // For nonlinear rules, we approximate the costs by 1 in every step, so they must never be 0
    // Note that we have to add this before searching for a metering function, since it has to hold in every step
    if (!rule.isLinear()) {
        rule.getGuardMut().push_back(rule.getCost() >= 1);
    }

    // Searching for metering functions works the same for linear and nonlinear rules
    MeteringFinder::Result meter = MeteringFinder::generate(varMan, rule);

    // If we fail, try again after instantiating temporary variables
    // (we always want to try this heuristic, since it is often applicable)
    if (meter.result == MeteringFinder::Unsat || meter.result == MeteringFinder::ConflictVar) {
        Rule instantiatedRule = rule;
        if (MeteringFinder::instantiateTempVarsHeuristic(varMan, instantiatedRule)) {
            meter = MeteringFinder::generate(varMan, instantiatedRule);
            rule = instantiatedRule;
        }
    }

    switch (meter.result) {
        case MeteringFinder::Nonlinear:
            res.result = TooComplicated;
            return res;

        case MeteringFinder::ConflictVar:
            res.result = NoMetering;
            conflictVar = meter.conflictVar;
            return res;

        case MeteringFinder::Unbounded:
        {
            // Since the loop is non-terminating, the right-hand sides are of no interest.
            rule.getCostMut() = Expression::InfSymbol;
            res.rules.emplace_back("NONTERM", rule.replaceRhssBySink(sink));
            res.result = Success;
            return res;
        }

        case MeteringFinder::Unsat:
            res.result = NoMetering;
            return res;

        case MeteringFinder::Success:
        {
            string meterStr = "metering function " + meter.metering.toString();

            // First apply the modifications required for this metering function
            Rule newRule = rule;
            if (meter.integralConstraint) {
                newRule.getGuardMut().push_back(meter.integralConstraint.get());
                meterStr += " (where " + meter.integralConstraint.get().toString() + ")";
            }

            if (newRule.isLinear()) {
                // Compute iterated cost/update by recurrence solving (modifies newRule)
                LinearRule linRule = newRule.toLinear();
                if (!Recurrence::iterateRule(varMan, linRule, meter.metering)) {
                    res.result = TooComplicated;
                    return res;
                }
                res.rules.emplace_back(meterStr, linRule);

            } else {
                // Compute the "iterated costs" by just assuming every step has cost 1
                int degree = newRule.rhsCount();
                Expression newCost = GiNaC::pow(degree, meter.metering);
                newRule.getCostMut() = (newCost - 1) / (degree - 1); // resulting cost is (d^meter-1)/(d-1)

                // We don't know to what result the rule evaluates (multiple rhss, so no single result).
                // So we have to clear the rhs (fresh sink location, update is irrelevant).
                res.rules.emplace_back(meterStr, newRule.replaceRhssBySink(sink));
            }

            res.result = Success;
            return res;
        }
    }

    unreachable();
}


option<MeteredRule> ForwardAcceleration::accelerateFast(VarMan &varMan, const Rule &rule, LocationIdx sink) {
    option<VariablePair> dummy;
    Result res = meterAndIterate(varMan, rule, sink, dummy);

    if (res.result == Success) {
        assert(res.rules.size() == 1);
        return res.rules.front();
    }

    return {};
}


Result ForwardAcceleration::accelerate(VarMan &varMan, const Rule &rule, LocationIdx sink) {
    // Try to find a metering function without any heuristics
    option<VariablePair> conflictVar;
    Result res = meterAndIterate(varMan, rule, sink, conflictVar);
    if (res.result != NoMetering) {
        return res; // either successful or there is no point in applying heuristics
    }

#ifdef METER_HEURISTIC_CONFLICTVAR
    // Apply the heuristic for conflicting variables (workaround as we don't support min(A,B) as metering function)
    if (conflictVar) {
        ExprSymbol A = varMan.getGinacSymbol(conflictVar->first);
        ExprSymbol B = varMan.getGinacSymbol(conflictVar->second);
        Rule newRule = rule;
        debugAccel("Trying MinMax heuristic with variables " << A << ", " << B << " for rule " << newRule);

        // Add A > B to the guard, try to accelerate
        newRule.getGuardMut().push_back(A > B);
        auto accelRule = accelerateFast(varMan, newRule, sink);
        if (accelRule) {
            debugAccel("MinMax heuristic (A > B) successful with rule: " << newRule);
            res.rules.push_back(accelRule.get().appendInfo(" (after adding " + Expression(A > B).toString() + ")"));
        }

        // Add A < B to the guard, try to accelerate
        newRule.getGuardMut().pop_back();
        newRule.getGuardMut().push_back(A < B);
        accelRule = accelerateFast(varMan, newRule, sink);
        if (accelRule) {
            debugAccel("MinMax heuristic (A < B) successful with rule: " << newRule);
            res.rules.push_back(accelRule.get().appendInfo(" (after adding " + Expression(A < B).toString() + ")"));
        }

        // Check if at least one attempt was successful.
        // If both were successful, then there is no real restriction (since we add both alternatives).
        if (!res.rules.empty()) {
            res.result = (res.rules.size() == 2) ? Success : SuccessWithRestriction;
            return res;
        }
    }
#endif

#ifdef METER_HEURISTIC_CONSTANT_UPDATE
    // Guard strengthening heuristic (helps in the presence of constant updates like x := 5).
    // Does not help very often, so we only apply it to nonlinear rules (since they can give exponential bounds).
    if (!rule.isLinear()) {
        Rule newRule = rule;
        debugAccel("Trying guard strengthening for rule: " << newRule);

        // Check and (possibly) apply heuristic, this modifies newRule
        if (MeteringFinder::strengthenGuard(varMan, newRule)) {
            auto accelRule = accelerateFast(varMan, newRule, sink);
            if (accelRule) {
                res.rules.push_back(accelRule.get().appendInfo(" (after strengthening guard)"));
                res.result = SuccessWithRestriction;
                return res;
            }
        }
    }
#endif

#ifdef METER_HEURISTIC_DROP_RHS
    // TODO: Delete some rhss of nonlinear rules and try again! (see paper step 3.2)
    // TODO: Trying all combinations is probably way too expensive, so maybe just drop the last one (repeatedly)?
#endif

    assert(res.result == NoMetering && res.rules.empty());
    return res;
}
