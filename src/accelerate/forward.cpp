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
#include "../debug.hpp"
#include "../util/stats.hpp"
#include "../util/timing.hpp"
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
    return MeteringFinder::generate(varMan, rule);
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
    proofout.increaseIndention();

    // Try to find a metering function
    MeteringFinder::Result meter = meterWithInstantiation(varMan, rule);

    switch (meter.result) {
        case MeteringFinder::Nonlinear:
            res.result = TooComplicated;
            Stats::add(Stats::MeterTooComplicated);
            return res;

        case MeteringFinder::ConflictVar:
            res.result = NoMetering;
            conflictVar = meter.conflictVar;
            Stats::add(Stats::MeterUnsat);
            return res;

        case MeteringFinder::Unsat:
            res.result = NoMetering;
            Stats::add(Stats::MeterUnsat);
            return res;

        case MeteringFinder::Success:
        case MeteringFinder::Nonterm:
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
                ExprSymbol iterationCount = varMan.getVarSymbol(varMan.addFreshTemporaryVariable("tv"));

                // Iterate cost and update
                LinearRule linRule = newRule.toLinear();
                const option<Recurrence::IteratedUpdates> &sol = Recurrence::iterateUpdates(varMan, {linRule.getUpdate()}, iterationCount);
                if (!sol || sol.get().validityBound > 1) {
                    res.result = NoMetering;
                    return res;
                }

                // The iterated update/cost computation is only sound if we do >= 1 iterations.
                // Hence we have to ensure that the metering function is >= 1 (corresponding to 0 < tv).
                linRule.getGuardMut().push_back(iterationCount >= sol.get().validityBound);

                linRule.getGuardMut().push_back(iterationCount <= meter.metering);

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

            res.result = SuccessWithRestriction;
            Stats::add(Stats::MeterSuccess);
            return res;
        }
        default:
            throw std::runtime_error("Unknown metering result!");
    }

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
    return meterAndIterate(varMan, rule, sink, conflictVar);
}
