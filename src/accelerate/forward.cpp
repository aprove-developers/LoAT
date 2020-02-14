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

#include "forward.hpp"

#include "recurrence/recurrence.hpp"
#include "meter/metering.hpp"
#include "../z3/z3toolbox.hpp"

#include "../global.hpp"
#include "../util/timeout.hpp"


using namespace std;
using ForwardAcceleration::Result;
using ForwardAcceleration::MeteredRule;


/**
 * Helper function that searches for a metering function and,
 * if not successful, tries to instantiate temporary variables.
 */
static MeteringFinder::Result meterWithInstantiation(VarMan &varMan, Rule &rule) {
    // Searching for metering functions works the same for linear and nonlinear rules
    MeteringFinder::Result meter = MeteringFinder::generate(varMan, rule);

    // If we fail, try again after instantiating temporary variables
    // (we always want to try this heuristic, since it is often applicable)
    if (Config::ForwardAccel::TempVarInstantiation) {
        if (meter.result == MeteringFinder::Unsat || meter.result == MeteringFinder::ConflictVar) {
            Rule instantiatedRule = rule;
            auto optRule = MeteringFinder::instantiateTempVarsHeuristic(varMan, rule);
            if (optRule) {
                rule = optRule.get();
                meter = MeteringFinder::generate(varMan, rule);
            }
        }
    }

    return meter;
}


/**
 * Helper function that calls meterWithInstantiation and, if successful,
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

    // We may require that the cost is at least 1 in every single iteration of the loop.
    // For linear rules, this is only required for non-termination (see special case below).
    // For nonlinear rules, we lower bound the costs by 1 for the iterated cost, so we always require this.
    // Note that we have to add this before searching for a metering function, since it has to hold in every step.
    if (!rule.isLinear()) {
        rule.getGuardMut().push_back(rule.getCost() >= 1);
    }

    // Try to find a metering function
    MeteringFinder::Result meter = meterWithInstantiation(varMan, rule);

    // In case of nontermination, we have to ensure that the costs are at least 1 in every step.
    // The reason is that an infinite iteration of a rule with cost 0 is not considered nontermination.
    // Since always adding "cost >= 1" may complicate the rule (if cost is nonlinear), we instead meter again.
    // (Note that instantiation has already been performed, but this is probably not a big issue at this point)
    if (meter.result == MeteringFinder::Nonterm && rule.isLinear()) {
        rule.getGuardMut().push_back(rule.getCost() >= 1);
        meter = meterWithInstantiation(varMan, rule);
    }

    switch (meter.result) {
        case MeteringFinder::Nonlinear:
            res.result = TooComplicated;
            return res;

        case MeteringFinder::ConflictVar:
            res.result = NoMetering;
            conflictVar = meter.conflictVar;
            return res;

        case MeteringFinder::Nonterm:
        {
            // Since the loop is non-terminating, the right-hand sides are of no interest.
            rule.getCostMut() = Expression::NontermSymbol;
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
                // Compute iterated cost/update by recurrence solving (modifies newRule).
                // Note that we usually assume that the maximal number of iterations is taken, so
                // instead of adding 0 < tv < meter+1 as in the paper, we instantiate tv by meter.
                Expression iterationCount = meter.metering;
                if (Config::ForwardAccel::UseTempVarForIterationCount) {
                    iterationCount = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("tv"));
                }

                // Iterate cost and update
                LinearRule linRule = newRule.toLinear();
                if (Recurrence::iterateRule(varMan, linRule, iterationCount)) {
                    res.result = TooComplicated;
                    return res;
                }

                // The iterated update/cost computation is only sound if we do >= 1 iterations.
                // Hence we have to ensure that the metering function is >= 1 (corresponding to 0 < tv).
                linRule.getGuardMut().push_back(iterationCount >= 1);

                // If we use a temporary variable instead of the metering function, add the upper bound.
                // Note that meter always maps to int, so we can use <= here.
                if (Config::ForwardAccel::UseTempVarForIterationCount) {
                    linRule.getGuardMut().push_back(iterationCount <= meter.metering);
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

    assert(false && "unreachable");
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

    // Apply the heuristic for conflicting variables (workaround as we don't support min(A,B) as metering function)
    if (Config::ForwardAccel::ConflictVarHeuristic && conflictVar) {
        ExprSymbol A = varMan.getVarSymbol(conflictVar->first);
        ExprSymbol B = varMan.getVarSymbol(conflictVar->second);
        Rule newRule = rule;

        // Add A >= B to the guard, try to accelerate (unless it becomes unsat due to the new constraint)
        newRule.getGuardMut().push_back(A >= B);
        if (Z3Toolbox::checkAll(newRule.getGuard()) != z3::unsat) {
            auto accelRule = accelerateFast(varMan, newRule, sink);
            if (accelRule) {
                res.rules.push_back(accelRule.get().appendInfo(" (after adding " + Expression(A >= B).toString() + ")"));
            }
        }

        // Add A <= B to the guard, try to accelerate (unless it becomes unsat due to the new constraint)
        newRule.getGuardMut().pop_back();
        newRule.getGuardMut().push_back(A <= B);
        if (Z3Toolbox::checkAll(newRule.getGuard()) != z3::unsat) {
            auto accelRule = accelerateFast(varMan, newRule, sink);
            if (accelRule) {
                res.rules.push_back(accelRule.get().appendInfo(" (after adding " + Expression(A <= B).toString() + ")"));
            }
        }

        // Check if at least one attempt was successful.
        // If both were successful, then there is no real restriction (since we add both alternatives).
        if (!res.rules.empty()) {
            res.result = (res.rules.size() == 2) ? Success : SuccessWithRestriction;
            return res;
        }
    }

    // Guard strengthening heuristic (helps in the presence of constant updates like x := 5 or x := free).
    if (Config::ForwardAccel::ConstantUpdateHeuristic) {
        Rule newRule = rule;

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

    assert(res.result == NoMetering && res.rules.empty());
    return res;
}
