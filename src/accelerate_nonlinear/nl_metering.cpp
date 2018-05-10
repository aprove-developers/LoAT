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

#include "nl_metering.h"

#include "accelerate/farkas.h"
#include "nl_linearize.h"
#include "nl_metertools.h"

#include "util/timing.h"
#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "expr/expression.h"
#include "z3/z3toolbox.h"
#include "util/timeout.h"

#include <boost/math/common_factor.hpp> // for gcd/lcdm

using namespace std;
using boost::optional;
namespace MT = MeteringToolboxNL;


MeteringFinderNL::MeteringFinderNL(VarMan &varMan, const GuardList &guard, const vector<UpdateMap> &updates)
    : varMan(varMan),
      updates(updates),
      guard(guard),
      absCoeff(context.addFreshVariable("c", Z3Context::Real))
{}


/* ### Step 1: Pre-processing, filter relevant constraints/variables ### */

void MeteringFinderNL::simplifyAndFindVariables() {
    irrelevantGuard.clear(); // clear in case this method is called twice
    reducedGuard = MT::reduceGuard(varMan, guard, updates, &irrelevantGuard);
    relevantVars = MT::findRelevantVariables(varMan, reducedGuard, updates);

    // Note that reducedGuard is already restricted by definition of relevantVars
    MT::restrictGuardToVariables(varMan, guard, relevantVars);
    MT::restrictGuardToVariables(varMan, irrelevantGuard, relevantVars);
    MT::restrictUpdatesToVariables(updates, relevantVars);
}

bool MeteringFinderNL::preprocessAndLinearize() {
    // preprocessing to avoid free variables
    MT::eliminateTempVars(varMan, guard, updates);
    guard = MT::replaceEqualities(guard);

    // simplify guard/update before linearization (expensive, but might remove nonlinear constraints)
    simplifyAndFindVariables();

    // linearize (try to substitute nonlinear parts)
    auto optSubs = LinearizeNL::linearizeGuardUpdates(varMan, guard, updates);
    if (optSubs) {
        nonlinearSubs = optSubs.get();
    } else {
        return false; // not able to linearize everything
    }

    // simplify guard/update again, if linearization has modified anything
    if (!nonlinearSubs.empty()) {
        simplifyAndFindVariables();
    }
    return true;
}


/* ### Step 2: Construction of linear constraints and metering function template ### */

void MeteringFinderNL::buildMeteringVariables() {
    // clear generated fields in case this method is called twice
    meterVars.symbols.clear();
    meterVars.coeffs.clear();
    meterVars.primedSymbols.clear();

#ifdef FARKAS_ALLOW_REAL_COEFFS
    Z3Context::VariableType coeffType = Z3Context::Real;
#else
    Z3Context::VariableType coeffType = Z3Context::Integer;
#endif

    for (VariableIdx var : relevantVars) {
        meterVars.symbols.push_back(varMan.getGinacSymbol(var));
        meterVars.coeffs.push_back(context.addFreshVariable("c", coeffType));
    }

    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            assert(relevantVars.count(it.first) > 0); // update should have been restricted to relevant variables

            if (meterVars.primedSymbols.count(it.first) == 0) {
                string primedName = varMan.getVarName(it.first)+"'";
                ExprSymbol primed = varMan.getFreshUntrackedSymbol(primedName);
                meterVars.primedSymbols.emplace(it.first, primed);
            }
        }
    }
}

void MeteringFinderNL::buildLinearConstraints() {
    // clear generated fields in case this method is called twice
    linearConstraints.guard.clear();
    linearConstraints.guardUpdate.clear();
    linearConstraints.reducedGuard.clear();
    linearConstraints.reducedGuard.clear();

    // guardUpdate will consist of as many constraint lists as there are updates
    linearConstraints.guardUpdate.resize(updates.size());

    // helper lambda to transform the given inequality into the required form
    auto makeConstraint = [&](const GiNaC::ex &rel, vector<Expression> &vec) {
        using namespace Relation;
        assert(isLinearInequality(rel,varMan.getGinacVarList()));
        Expression res = splitVariablesAndConstants(toLessEq(rel));
        if (!isTrivialLessEqInequality(res)) {
            vec.push_back(res);
        }
    };

    for (Expression ex : reducedGuard) {
        makeConstraint(ex, linearConstraints.reducedGuard);
    }

    for (Expression ex : irrelevantGuard) {
        makeConstraint(ex, linearConstraints.irrelevantGuard);
    }

    for (Expression ex : guard) {
        makeConstraint(ex, linearConstraints.guard);

        // all of the guardUpdate constraints need to include the guard
        for (GuardList &vec : linearConstraints.guardUpdate) {
            makeConstraint(ex, vec);
        }
    }

    for (int i=0; i < updates.size(); ++i) {
        for (const auto &it : updates[i]) {
            assert(meterVars.primedSymbols.count(it.first) > 0);
            ExprSymbol primed = meterVars.primedSymbols.at(it.first);

            makeConstraint(primed <= it.second, linearConstraints.guardUpdate[i]);
            makeConstraint(primed >= it.second, linearConstraints.guardUpdate[i]);
        }
    }

#ifdef DEBUG_METERING
    debugMeter("Resulting linear constraints:");
    dumpList("guard           ", linearConstraints.guard);
    dumpList("reduced guard   ", linearConstraints.reducedGuard);
    dumpList("irrelevant guard", linearConstraints.irrelevantGuard);
    for (int i=0; i < updates.size(); ++i) {
        dumpList("update ["+to_string(i)+"]", linearConstraints.guardUpdate[i]);
    }
#endif
}


/* ### Step 3: Construction of the final constraints for the metering function using Farkas lemma ### */

z3::expr MeteringFinderNL::genNotGuardImplication() const {
    debugMeter("Constructing not-guard implication");
    vector<z3::expr> res;
    vector<Expression> lhs;

    // split into one implication for every guard constraint, apply Farkas for each implication
    for (const Expression &g : linearConstraints.reducedGuard) {
        lhs.push_back(Relation::negateLessEqInequality(g));
        res.push_back(FarkasLemma::apply(lhs, meterVars.symbols, meterVars.coeffs, absCoeff, 0, context));
        lhs.pop_back();
    }

    return Z3Toolbox::concat(context, res, Z3Toolbox::ConcatAnd);
}

z3::expr MeteringFinderNL::genGuardPositiveImplication(bool strict) const {
    debugMeter("Constructing guard positive implication (strict = " << strict << ")");
    //G ==> f(x) > 0, which is equivalent to -f(x) < 0  ==  -f(x) <= -1 (on integers)
    vector<z3::expr> negCoeff;
    for (const z3::expr &coeff : meterVars.coeffs) {
        negCoeff.push_back(-coeff);
    }

    int delta = strict ? -1 : -0;
    return FarkasLemma::apply(linearConstraints.guard, meterVars.symbols, negCoeff, -absCoeff, delta, context);
}

z3::expr MeteringFinderNL::genUpdateImplications() const {
    debugMeter("Constructing update implication");

    // For each update, build f(x)-f(x') => x-x'
    // Note that we only include the (primed) variables actually affected by the update.
    // The other variables can be left out to simplify the z3 query (since they cancel out)

    vector<z3::expr> res;
    for (int updateIdx=0; updateIdx < updates.size(); ++updateIdx) {
        vector<ExprSymbol> vars;
        vector<z3::expr> coeffs;

        for (int i=0; i < meterVars.symbols.size(); ++i) {
            ExprSymbol sym = meterVars.symbols[i];
            VariableIdx var = varMan.getVarIdx(sym);
            z3::expr coeff = meterVars.coeffs[i];

            // ignore variables not affected by the current update
            if (!updates[updateIdx].isUpdated(var)) continue;

            // find the primed version of sym
            assert(meterVars.primedSymbols.count(var) > 0);
            ExprSymbol primed = meterVars.primedSymbols.at(var);

            vars.push_back(sym);      //x
            vars.push_back(primed);   //x'
            coeffs.push_back( coeff); //coeff for x
            coeffs.push_back(-coeff); //coeff for x', i.e. negative coeff for x
        }

        // the absolute coefficient also cancels out, so we set it to 0
        z3::expr zeroAbsCoeff = context.real_val(0);
        res.push_back(FarkasLemma::apply(linearConstraints.guardUpdate[updateIdx], vars, coeffs, zeroAbsCoeff, 1, context));
    }

    // all implications have to hold for the metering function
    return Z3Toolbox::concat(context, res, Z3Toolbox::ConcatAnd);
}

z3::expr MeteringFinderNL::genNonTrivial() const {
    debugMeter("Constructing non-trivial constraint");
    vector<z3::expr> res;
    for (const z3::expr &c : meterVars.coeffs) {
        res.push_back(c != 0);
    }
    return Z3Toolbox::concat(context, res, Z3Toolbox::ConcatOr);
}


/* ### Step 4: Result and model interpretation ### */

Expression MeteringFinderNL::buildResult(const z3::model &model) const {
    const auto &coeffs = meterVars.coeffs;
    const auto &symbols = meterVars.symbols;

    // read off the coefficients of the metering function
    Expression result = Z3Toolbox::getRealFromModel(model, absCoeff);
    for (int i=0; i < coeffs.size(); ++i) {
        result = result + Z3Toolbox::getRealFromModel(model,coeffs[i]) * symbols[i];
    }
    debugMeter("Result before substitution: " << result);

    // reverse linearization
    result.applySubs(nonlinearSubs);
    debugMeter("Result after substitution: " << result);
    return result;
}

void MeteringFinderNL::ensureIntegralMetering(Result &result, const z3::model &model) const {
    bool has_reals = false;
    int mult = 1;

    for (const z3::expr &z3coeff : meterVars.coeffs) {
        GiNaC::numeric coeff = GiNaC::ex_to<GiNaC::numeric>(Z3Toolbox::getRealFromModel(model, z3coeff));
        if (coeff.denom().to_int() != 1) {
            has_reals = true;
            mult = boost::math::lcm(mult, coeff.denom().to_int());
        }
    }

    // remove reals by multiplying metering function with "mult",
    // then add a fresh variable that corresponds to the original value of the metering function
    if (has_reals) {
        VariableIdx tempIdx = varMan.addFreshTemporaryVariable("meter");
        ExprSymbol tempVar = varMan.getGinacSymbol(tempIdx);

        // create a new guard constraint relating tempVar and the metering function
        result.integralConstraint = (tempVar*mult == result.metering*mult);

        // replace the metering function by tempVar
        result.metering = tempVar;

        debugMeter("Has reals, adding constraint: " << result.integralConstraint.get());
    }
}

optional<VariablePair> MeteringFinderNL::findConflictVars() const {
    set<VariableIdx> conflictingVars;

    // find variables on which the loop's runtime might depend (simple heuristic)
    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            ExprSymbol lhsVar = varMan.getGinacSymbol(it.first);
            ExprSymbolSet rhsVars = it.second.getVariables();

            // the update must be some sort of simple counting, e.g. A = A+2
            if (rhsVars.size() != 1 || rhsVars.count(lhsVar) == 0) continue;

            // and there must be a guard term limiting the execution of this counting
            for (const Expression &ex : reducedGuard) {
                if (ex.has(lhsVar)) {
                    conflictingVars.insert(it.first);
                    break;
                }
            }
        }
    }

    // we limit the heuristic to only handle 2 variables
    if (conflictingVars.size() == 2) {
        auto it = conflictingVars.begin();
        VariableIdx a = *(it++);
        VariableIdx b = *it;

        debugProblem("Metering found conflicting variables: " << varMan.getGinacSymbol(a) << " and " << varMan.getGinacSymbol(b));
        return make_pair(a, b);
    }

    return {};
}


/* ### Main function ### */

void MeteringFinderNL::dump(const string &msg) const {
#ifdef DEBUG_METERING
    debugMeter("### Metering: " << msg << " ###");

    stringstream ss;
    for (VariableIdx var : relevantVars) {
        ss << " " << var << "/" << varMan.getGinacSymbol(var);
    }
    debugMeter("Relevant variables: " << ss.str());

    dumpList("guard           ", guard);
    dumpList("reduced guard   ", reducedGuard);
    dumpList("irrelevant guard", irrelevantGuard);
    dumpMaps("updates", updates);
#endif
}


MeteringFinderNL::Result MeteringFinderNL::generate(VarMan &varMan, const NonlinearRule &rule) {
    Timing::Scope timer(Timing::FarkasTotal);
    Timing::start(Timing::FarkasLogic);

    Result result;
    vector<UpdateMap> updates;
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        updates.push_back(rhs->getUpdate());
    }

    MeteringFinderNL meter(varMan, rule.getGuard(), updates);

    meter.dump("Initial");

    // linearize and simplify the problem
    if (!meter.preprocessAndLinearize()) {
        Timing::done(Timing::FarkasLogic);
        result.result = Nonlinear;
        return result;
    }

    meter.dump("After Preprocess");

    // identify trivially unbounded loops (no guard constraint is limiting the loop's execution)
    if (meter.reducedGuard.empty()) {
        Timing::done(Timing::FarkasLogic);
        result.result = Unbounded;
        return result;
    }

    // create constraints for the metering function template
    meter.buildMeteringVariables();
    meter.buildLinearConstraints();
    Timing::done(Timing::FarkasLogic);

    // solve constraints for the metering function (without the "GuardPositiveImplication" for now)
    Z3Solver solver(meter.context);
    solver.add(meter.genNotGuardImplication());
    solver.add(meter.genUpdateImplications());
    solver.add(meter.genNonTrivial());
    z3::check_result z3res = solver.check();

    // the problem is already unsat (even without "GuardPositiveImplication")
    if (z3res == z3::unsat) {
        debugMeter("z3 pre unsat");
        debugProblem("Farkas pre unsat for: " << rule);

#ifdef FARKAS_HEURISTIC_FOR_MINMAX
        auto conflictVar = meter.findConflictVars();
        if (conflictVar) {
            result.conflictVar = conflictVar.get();
            result.result = ConflictVar;
            return result;
        }
#endif
        result.result = Unsat;
        return result;
    }

    // Add the "GuardPositiveImplication" to the party (first the strict version)
    solver.push();
    solver.add(meter.genGuardPositiveImplication(true));
    z3res = solver.check();

    // If we fail, try the relaxed version instead (f(x) >= 0 instead of f(x) > 0)
    if (z3res != z3::sat) {
        debugMeter("z3 strict positive: " << z3res);
        debugProblem("Farkas strict positive is " << z3res << " for: " << rule);
        solver.pop();
        solver.add(meter.genGuardPositiveImplication(false));
        z3res = solver.check();
    }

    // If we still fail, we have to give up
    if (z3res != z3::sat) {
        debugMeter("z3 final res: " << z3res);
        debugProblem("Farkas final result " << z3res << " for: " << rule);
        result.result = Unsat;
        return result;
    }

    // If we succeed, extract the metering function from the model
    z3::model model = solver.get_model();
    debugMeter("Success, z3 model: " << model);
    result.metering = meter.buildResult(model);
    result.result = Success;

#ifdef FARKAS_ALLOW_REAL_COEFFS
    // If we allow real coefficients, we have to be careful that the metering function evaluates to an integer.
    meter.ensureIntegralMetering(result, model);
#endif

    return result;
}



/* ### Heuristics to help finding more metering functions ### */

bool MeteringFinderNL::strengthenGuard(VarMan &varMan, NonlinearRule &rule) {
    vector<UpdateMap> updates;
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        updates.push_back(rhs->getUpdate());
    }
    return MeteringToolboxNL::strengthenGuard(varMan, rule.getGuardMut(), updates);
}

bool MeteringFinderNL::instantiateTempVarsHeuristic(VarMan &varMan, NonlinearRule &rule) {
    vector<UpdateMap> updates;
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        updates.push_back(rhs->getUpdate());
    }

    MeteringFinderNL meter(varMan, rule.getGuard(), updates);

    // We first perform the same steps as in generate()

    if (!meter.preprocessAndLinearize()) return {};
    assert(!meter.reducedGuard.empty()); // this method must only be called if generate() fails

    meter.buildMeteringVariables();
    meter.buildLinearConstraints();

    Z3Solver solver(meter.context);
    solver.add(meter.genNotGuardImplication());
    solver.add(meter.genUpdateImplications());
    solver.add(meter.genNonTrivial());
    z3::check_result z3res = solver.check();
    assert(z3res == z3::unsat); // this method must only be called if generate() fails

    // Now try all possible instantiations until the solver is satisfied

    GuardList oldGuard = meter.guard;
    vector<UpdateMap> oldUpdates = meter.updates;

    GiNaC::exmap successfulSubs;
    stack<GiNaC::exmap> freeSubs = MeteringToolboxNL::findInstantiationsForTempVars(varMan, meter.guard);

    while (!freeSubs.empty()) {
        if (Timeout::soft()) break;

        const GiNaC::exmap &sub = freeSubs.top();
        debugFarkas("Trying instantiation: " << sub);

        //apply current substitution (and forget the previous one)
        meter.guard.clear();
        for (Expression &ex : oldGuard) meter.guard.push_back(ex.subs(sub));

        meter.updates.clear();
        for (const UpdateMap &oldUpdate : oldUpdates) {
            UpdateMap update;
            for (const auto &up : oldUpdate) {
                update[up.first] = up.second.subs(sub);
            }
            meter.updates.push_back(update);
        }

        // Perform the first steps from generate() again (guard/update have changed)
        meter.simplifyAndFindVariables();
        meter.buildMeteringVariables();
        meter.buildLinearConstraints();

        solver.reset();
        solver.add(meter.genNotGuardImplication());
        solver.add(meter.genUpdateImplications());
        solver.add(meter.genNonTrivial());
        z3res = solver.check();

        if (z3res == z3::sat) {
            successfulSubs = sub;
            break;
        }

        freeSubs.pop();
    }

    // If we found a successful instantiation, z3res is sat
    if (z3res == z3::unsat) {
        return false;
    }

    // Apply the successful instantiation to the entire rule
    for (Expression &ex : rule.getGuardMut()) {
        ex.applySubs(successfulSubs);
    }
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        for (auto &it : rhs->getUpdateMut()) {
            it.second.applySubs(successfulSubs);
        }
    }
    rule.getCostMut().applySubs(successfulSubs);

    return true;
}
