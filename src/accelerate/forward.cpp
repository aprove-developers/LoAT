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
#include "../smt/smt.hpp"


using namespace std;
using ForwardAcceleration::Result;


/**
 * Helper function that searches for a metering function and,
 * if not successful, tries to instantiate temporary variables.
 */
static MeteringFinder::Result meterWithInstantiation(ITSProblem &its, const Rule &rule) {
    // Searching for metering functions works the same for linear and nonlinear rules
    MeteringFinder::Result meter = MeteringFinder::generate(its, rule);

    // If we fail, try again after instantiating temporary variables
    // (we always want to try this heuristic, since it is often applicable)
    if (Config::ForwardAccel::TempVarInstantiation) {
        if (meter.result == MeteringFinder::Unsat || meter.result == MeteringFinder::ConflictVar) {
            Rule instantiatedRule = rule;
            auto optRule = MeteringFinder::instantiateTempVarsHeuristic(its, rule);
            if (optRule) {
                meter.rule = optRule.get().first;
                ProofOutput proof = optRule.get().second;
                meter = MeteringFinder::generate(its, rule);
                proof.concat(meter.proof);
                meter.proof = proof;
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
static Result meterAndIterate(ITSProblem &its, const Rule &r, LocationIdx sink, option<std::pair<Var, Var>> &conflictVar) {
    using namespace ForwardAcceleration;
    Result res;

    // We may require that the cost is at least 1 in every single iteration of the loop.
    // For linear rules, this is only required for non-termination (see special case below).
    // For nonlinear rules, we lower bound the costs by 1 for the iterated cost, so we always require this.
    // Note that we have to add this before searching for a metering function, since it has to hold in every step.
    Rule rule = r.isLinear() ? r: r.withGuard(r.getGuard() & (r.getCost() >= 1));

    // Try to find a metering function
    MeteringFinder::Result meter = meterWithInstantiation(its, rule);
    if (meter.rule) {
        rule = meter.rule.get();
    }

    // In case of nontermination, we have to ensure that the costs are at least 1 in every step.
    // The reason is that an infinite iteration of a rule with cost 0 is not considered nontermination.
    // Since always adding "cost >= 1" may complicate the rule (if cost is nonlinear), we instead meter again.
    // (Note that instantiation has already been performed, but this is probably not a big issue at this point)
    if (meter.result == MeteringFinder::Nonterm && rule.isLinear()) {
        rule = rule.withGuard(rule.getGuard() & (rule.getCost() >= 1));
        meter = meterWithInstantiation(its, rule);
        if (meter.rule) {
            rule = meter.rule.get();
        }
    }

    switch (meter.result) {
        case MeteringFinder::Nonlinear:
            res.status = Failure;
            return res;

        case MeteringFinder::ConflictVar:
            res.status = Failure;
            conflictVar = meter.conflictVar;
            return res;

        case MeteringFinder::Nonterm:
        {
            // Since the loop is non-terminating, the right-hand sides are of no interest.
            const Rule &nontermRule = rule.withCost(Expr::NontermSymbol).replaceRhssBySink(sink);
            res.proof.concat(meter.proof);
            res.proof.ruleTransformationProof(rule, "Proved universal non-termiation while searching metering function", nontermRule, its);
            res.rules.emplace_back(nontermRule);
            res.status = Success;
            return res;
        }

        case MeteringFinder::Unsat:
            res.status = Failure;
            return res;

        case MeteringFinder::Success:
        {
            string meterStr = meter.metering.toString();

            // First apply the modifications required for this metering function
            Rule newRule = rule;
            if (meter.integralConstraint) {
                newRule = newRule.withGuard(newRule.getGuard() & meter.integralConstraint.get());
                meterStr += " (where " + meter.integralConstraint.get().toString() + ")";
            }

            if (newRule.isLinear()) {
                // Compute iterated cost/update by recurrence solving (modifies newRule).
                // Note that we usually assume that the maximal number of iterations is taken, so
                // instead of adding 0 < tv < meter+1 as in the paper, we instantiate tv by meter.
                Expr iterationCount = meter.metering;
                if (Config::ForwardAccel::UseTempVarForIterationCount) {
                    iterationCount = its.addFreshTemporaryVariable("tv");
                }

                // Iterate cost and update
                LinearRule linRule = newRule.toLinear();
                const option<Recurrence::Result> &recurrenceRes = Recurrence::iterateRule(its, linRule, iterationCount);
                if (!recurrenceRes) {
                    res.status = Failure;
                    return res;
                }

                GuardList newGuard(rule.getGuard());
                newGuard.push_back(iterationCount >= recurrenceRes->validityBound);
                // If we use a temporary variable instead of the metering function, add the upper bound.
                // Note that meter always maps to int, so we can use <= here.
                if (Config::ForwardAccel::UseTempVarForIterationCount) {
                    newGuard.push_back(iterationCount <= meter.metering);
                }
                LinearRule newRule(linRule.getLhsLoc(), newGuard, recurrenceRes->cost, linRule.getRhsLoc(), recurrenceRes->update);
                res.proof.concat(meter.proof);
                res.proof.ruleTransformationProof(rule, "Acceleration with metering function " + meterStr, newRule, its);
                res.rules.emplace_back(newRule);

            } else {
                // Compute the "iterated costs" by just assuming every step has cost 1
                size_t degree = newRule.rhsCount();
                Expr newCost = degree ^ meter.metering;
                newRule = newRule.withCost((newCost - 1) / (degree - 1)); // resulting cost is (d^meter-1)/(d-1)

                // We don't know to what result the rule evaluates (multiple rhss, so no single result).
                // So we have to clear the rhs (fresh sink location, update is irrelevant).
                const Rule &accelRule = newRule.replaceRhssBySink(sink);
                res.proof.ruleTransformationProof(rule, "Acceleration with metering function " + meterStr, accelRule, its);
                res.rules.emplace_back(accelRule);
            }

            res.status = Success;
            return res;
        }
    }

    assert(false && "unreachable");
}


Result ForwardAcceleration::accelerateFast(ITSProblem &its, const Rule &rule, LocationIdx sink) {
    option<std::pair<Var, Var>> dummy;
    return meterAndIterate(its, rule, sink, dummy);
}


Result ForwardAcceleration::accelerate(ITSProblem &its, const Rule &rule, LocationIdx sink) {
    // Try to find a metering function without any heuristics
    option<std::pair<Var, Var>> conflictVar;
    Result accel = meterAndIterate(its, rule, sink, conflictVar);
    if (accel.status != Failure) {
        return accel;
    }

    Result res;
    // Apply the heuristic for conflicting variables (workaround as we don't support min(A,B) as metering function)
    if (Config::ForwardAccel::ConflictVarHeuristic && conflictVar) {
        Var A = conflictVar->first;
        Var B = conflictVar->second;

        // Add A >= B to the guard, try to accelerate (unless it becomes unsat due to the new constraint)
        Rule newRule = rule.withGuard(rule.getGuard() & (A >= B));
        if (Smt::check(buildAnd(newRule.getGuard()), its) != Smt::Unsat) {
            const Result &accel = accelerateFast(its, newRule, sink);
            if (accel.status != Failure) {
                res.proof.ruleTransformationProof(rule, "strengthening", newRule, its);
                res.proof.concat(accel.proof);
                res.rules.insert(res.rules.end(), accel.rules.begin(), accel.rules.end());
            }
        }

        // Add A <= B to the guard, try to accelerate (unless it becomes unsat due to the new constraint)
        newRule = rule.withGuard(rule.getGuard() & (A <= B));
        if (Smt::check(buildAnd(newRule.getGuard()), its) != Smt::Unsat) {
            const Result &accel = accelerateFast(its, newRule, sink);
            if (accel.status != Failure) {
                res.proof.ruleTransformationProof(rule, "strengthening", newRule, its);
                res.proof.concat(accel.proof);
                res.rules.insert(res.rules.end(), accel.rules.begin(), accel.rules.end());
            }
        }

        // Check if at least one attempt was successful.
        // If both were successful, then there is no real restriction (since we add both alternatives).
        if (!res.rules.empty()) {
            res.status = (res.rules.size() == 2) ? Success : PartialSuccess;
            return res;
        }
    }

    // Guard strengthening heuristic (helps in the presence of constant updates like x := 5 or x := free).
    if (Config::ForwardAccel::ConstantUpdateHeuristic) {
        // Check and (possibly) apply heuristic, this modifies newRule
        option<Rule> strengthened = MeteringFinder::strengthenGuard(its, rule);
        if (strengthened) {
            const Result &accel = accelerateFast(its, strengthened.get(), sink);
            if (accel.status != Failure) {
                res.proof.ruleTransformationProof(rule, "strengthening", strengthened.get(), its);
                res.proof.concat(accel.proof);
                res.rules.insert(res.rules.end(), accel.rules.begin(), accel.rules.end());
                res.status = PartialSuccess;
                return res;
            }
        }
    }

    assert(res.status == Failure && res.rules.empty());
    return res;
}
