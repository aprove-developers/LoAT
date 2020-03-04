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

#include "metering.hpp"

#include "farkas.hpp"
#include "linearize.hpp"
#include "metertools.hpp"

#include "../../expr/guardtoolbox.hpp"
#include "../../expr/relation.hpp"
#include "../../expr/expression.hpp"
#include "../../smt/smt.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../expr/boolexpr.hpp"
#include "../../util/timeout.hpp"
#include "../../util/proofoutput.hpp"

#include <boost/integer/common_factor.hpp> // for lcm

using namespace std;
namespace MT = MeteringToolbox;


MeteringFinder::MeteringFinder(VarMan &varMan, const GuardList &guard, const vector<UpdateMap> &updates)
    : varMan(varMan),
      updates(updates),
      guard(guard),
      absCoeff(varMan.getFreshUntrackedSymbol("c", Expression::Real))
{}


/* ### Helpers ### */

vector<UpdateMap> MeteringFinder::getUpdateList(const Rule &rule) {
    vector<UpdateMap> res;
    res.reserve(rule.rhsCount());
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        res.push_back(rhs->getUpdate());
    }
    return res;
}


/* ### Step 1: Pre-processing, filter relevant constraints/variables ### */

void MeteringFinder::simplifyAndFindVariables() {
    irrelevantGuard.clear(); // clear in case this method is called twice
    reducedGuard = MT::reduceGuard(varMan, guard, updates, &irrelevantGuard);
    relevantVars = MT::findRelevantVariables(varMan, reducedGuard, updates);

    // Note that reducedGuard is already restricted by definition of relevantVars
    MT::restrictGuardToVariables(varMan, guard, relevantVars);
    MT::restrictGuardToVariables(varMan, irrelevantGuard, relevantVars);
    MT::restrictUpdatesToVariables(updates, relevantVars);
}

bool MeteringFinder::preprocessAndLinearize() {
    // simplify guard/update before linearization (expensive, but might remove nonlinear constraints)
    guard = MT::replaceEqualities(guard);
    simplifyAndFindVariables();

    // linearize (try to substitute nonlinear parts)
    auto optSubs = Linearize::linearizeGuardUpdates(varMan, guard, updates);
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

void MeteringFinder::buildMeteringVariables() {
    // clear generated fields in case this method is called twice
    meterVars.symbols.clear();
    meterVars.coeffs.clear();
    meterVars.primedSymbols.clear();

    auto coeffType = (Config::ForwardAccel::AllowRealCoeffs) ? Expression::Real : Expression::Int;

    for (VariableIdx var : relevantVars) {
        meterVars.symbols.push_back(varMan.getVarSymbol(var));
        meterVars.coeffs.push_back(varMan.getFreshUntrackedSymbol("c", coeffType));
    }

    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            assert(relevantVars.count(it.first) > 0); // update should have been restricted to relevant variables

            if (meterVars.primedSymbols.count(it.first) == 0) {
                string primedName = varMan.getVarName(it.first)+"'";
                ExprSymbol primed = varMan.getFreshUntrackedSymbol(primedName, Expression::Int);
                meterVars.primedSymbols.emplace(it.first, primed);
            }
        }
    }
}

void MeteringFinder::buildLinearConstraints() {
    // clear generated fields in case this method is called twice
    linearConstraints.guard.clear();
    linearConstraints.guardUpdate.clear();
    linearConstraints.reducedGuard.clear();
    linearConstraints.reducedGuard.clear();

    // guardUpdate will consist of as many constraint lists as there are updates
    linearConstraints.guardUpdate.resize(updates.size());

    // helper lambda to transform the given inequality into the required form
    auto makeConstraint = [&](const Expression &rel, vector<Expression> &vec) {
        using namespace Relation;
        assert(isLinearInequality(rel));

        Expression res = splitVariablesAndConstants(toLessEq(rel));
        if (!isTriviallyTrue(res)) {
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

    for (unsigned int i=0; i < updates.size(); ++i) {
        for (const auto &it : updates[i]) {
            assert(meterVars.primedSymbols.count(it.first) > 0);
            ExprSymbol primed = meterVars.primedSymbols.at(it.first);

            makeConstraint(primed <= it.second, linearConstraints.guardUpdate[i]);
            makeConstraint(primed >= it.second, linearConstraints.guardUpdate[i]);
        }
    }

}


/* ### Step 3: Construction of the final constraints for the metering function using Farkas lemma ### */

BoolExpr MeteringFinder::genNotGuardImplication() const {
    BoolExpr res;
    vector<Expression> lhs;

    // We can add the irrelevant guard to the lhs ("conditional metering function")
    if (Config::ForwardAccel::ConditionalMetering) {
        lhs = linearConstraints.irrelevantGuard;
    }

    // split into one implication for every guard constraint, apply Farkas for each implication
    for (const Expression &g : linearConstraints.reducedGuard) {
        lhs.push_back(Relation::negateLessEqInequality(g));
        res = res & FarkasLemma::apply(lhs, meterVars.symbols, meterVars.coeffs, absCoeff, 0, varMan);
        lhs.pop_back();
    }

    return res;
}

BoolExpr MeteringFinder::genGuardPositiveImplication(bool strict) const {
    //G ==> f(x) > 0, which is equivalent to -f(x) < 0  ==  -f(x) <= -1 (on integers)
    vector<Expression> negCoeff;
    for (const ExprSymbol &coeff : meterVars.coeffs) {
        negCoeff.push_back(-coeff);
    }

    int delta = strict ? -1 : -0;
    return FarkasLemma::apply(linearConstraints.guard, meterVars.symbols, negCoeff, -absCoeff, delta, varMan);
}

BoolExpr MeteringFinder::genUpdateImplications() const {
    // For each update, build f(x)-f(x') => x-x'
    // Note that we only include the (primed) variables actually affected by the update.
    // The other variables can be left out to simplify the z3 query (since they cancel out)

    BoolExpr res;
    for (unsigned int updateIdx=0; updateIdx < updates.size(); ++updateIdx) {
        vector<ExprSymbol> vars;
        vector<Expression> coeffs;

        for (unsigned int i=0; i < meterVars.symbols.size(); ++i) {
            ExprSymbol sym = meterVars.symbols[i];
            VariableIdx var = varMan.getVarIdx(sym);
            Expression coeff = meterVars.coeffs[i];

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
        Expression zeroAbsCoeff = Expression(0);
        res = res & FarkasLemma::apply(linearConstraints.guardUpdate[updateIdx], vars, coeffs, zeroAbsCoeff, 1, varMan);
    }

    return res;
}

BoolExpr MeteringFinder::genNonTrivial() const {
    BoolExpr res;
    for (const ExprSymbol &c : meterVars.coeffs) {
        res = res | (c != 0);
    }
    return res;
}


/* ### Step 4: Result and model interpretation ### */

Expression MeteringFinder::buildResult(const ExprSymbolMap<GiNaC::numeric> &model) const {
    const auto &coeffs = meterVars.coeffs;
    const auto &symbols = meterVars.symbols;

    // read off the coefficients of the metering function
    Expression result = model.at(absCoeff);
    for (unsigned int i=0; i < coeffs.size(); ++i) {
        result = result + model.at(coeffs[i]) * symbols[i];
    }
    // reverse linearization
    result.applySubs(nonlinearSubs);
    return result;
}

void MeteringFinder::ensureIntegralMetering(Result &result, const ExprSymbolMap<GiNaC::numeric> &model) const {
    bool has_reals = false;
    int mult = 1;

    for (const ExprSymbol &theCoeff : meterVars.coeffs) {
        GiNaC::numeric coeff = model.at(theCoeff);
        if (coeff.denom().to_int() != 1) {
            has_reals = true;
            mult = boost::integer::lcm(mult, coeff.denom().to_int());
        }
    }

    // remove reals by multiplying metering function with "mult",
    // then add a fresh variable that corresponds to the original value of the metering function
    if (has_reals) {
        VariableIdx tempIdx = varMan.addFreshTemporaryVariable("meter");
        ExprSymbol tempVar = varMan.getVarSymbol(tempIdx);

        // create a new guard constraint relating tempVar and the metering function
        result.integralConstraint = (tempVar*mult == result.metering*mult);

        // replace the metering function by tempVar
        result.metering = tempVar;
    }
}

option<VariablePair> MeteringFinder::findConflictVars() const {
    set<VariableIdx> conflictingVars;

    // find variables on which the loop's runtime might depend (simple heuristic)
    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            ExprSymbol lhsVar = varMan.getVarSymbol(it.first);
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
        return make_pair(a, b);
    }

    return {};
}


/* ### Main function ### */

MeteringFinder::Result MeteringFinder::generate(VarMan &varMan, const Rule &rule) {

    Result result;
    MeteringFinder meter(varMan, rule.getGuard(), getUpdateList(rule));

    // linearize and simplify the problem
    if (!meter.preprocessAndLinearize()) {
        result.result = Nonlinear;
        return result;
    }

    // identify trivially non-terminating loops (no guard constraint is limiting the loop's execution)
    if (meter.reducedGuard.empty()) {
        result.result = Nonterm;
        return result;
    }

    // create constraints for the metering function template
    meter.buildMeteringVariables();
    meter.buildLinearConstraints();

    // solve constraints for the metering function (without the "GuardPositiveImplication" for now)
    std::unique_ptr<Smt> solver = SmtFactory::modelBuildingSolver(Smt::LA, varMan, Config::Z3::MeterTimeout);
    solver->add(meter.genNotGuardImplication());
    solver->add(meter.genUpdateImplications());
    solver->add(meter.genNonTrivial());
    Smt::Result smtRes = solver->check();

    // the problem is already unsat (even without "GuardPositiveImplication")
    if (smtRes == Smt::Unsat) {

        if (Config::ForwardAccel::ConflictVarHeuristic) {
            auto conflictVar = meter.findConflictVars();
            if (conflictVar) {
                result.conflictVar = conflictVar.get();
                result.result = ConflictVar;
                return result;
            }
        }

        result.result = Unsat;
        return result;
    }

    // Add the "GuardPositiveImplication" to the party (first the strict version)
    solver->push();
    solver->add(meter.genGuardPositiveImplication(true));
    smtRes = solver->check();

    // If we fail, try the relaxed version instead (f(x) >= 0 instead of f(x) > 0)
    if (smtRes != Smt::Sat) {
        solver->pop();
        solver->add(meter.genGuardPositiveImplication(false));
        smtRes = solver->check();
    }

    // If we still fail, we have to give up
    if (smtRes != Smt::Sat) {
        result.result = Unsat;
        return result;
    }

    // If we succeed, extract the metering function from the model
    ExprSymbolMap<GiNaC::numeric> model = solver->model();
    result.metering = meter.buildResult(model);
    result.result = Success;

    // If we allow real coefficients, we have to be careful that the metering function evaluates to an integer.
    if (Config::ForwardAccel::AllowRealCoeffs) {
        meter.ensureIntegralMetering(result, model);
    }

    // Proof output for linearization (since this is an addition to the paper)
    if (!meter.nonlinearSubs.empty()) {
        result.proof.section("Applied linearization");
        result.proof.append(stringstream() << "Linearized rule by temporarily substituting " << meter.nonlinearSubs);
    }

    return result;
}



/* ### Heuristics to help finding more metering functions ### */

bool MeteringFinder::strengthenGuard(VarMan &varMan, Rule &rule) {
    return MT::strengthenGuard(varMan, rule.getGuardMut(), getUpdateList(rule));
}

option<pair<Rule, ProofOutput>> MeteringFinder::instantiateTempVarsHeuristic(ITSProblem &its, const Rule &rule) {
    // Quick check whether there are any bounds on temp vars we can use to instantiate them.
    auto hasTempVar = [&](const Expression &ex) { return GuardToolbox::containsTempVar(its, ex); };
    if (std::none_of(rule.getGuard().begin(), rule.getGuard().end(), hasTempVar)) {
        return {};
    }

    // We first perform the same steps as in generate()
    MeteringFinder meter(its, rule.getGuard(), getUpdateList(rule));

    if (!meter.preprocessAndLinearize()) return {};
    assert(!meter.reducedGuard.empty()); // this method must only be called if generate() fails

    meter.buildMeteringVariables();
    meter.buildLinearConstraints();

    std::unique_ptr<Smt> solver = SmtFactory::solver(Smt::LA, its, Config::Z3::MeterTimeout);
    Smt::Result smtRes = Smt::Unsat; // this method should only be called if generate() fails

    GuardList oldGuard = meter.guard;
    vector<UpdateMap> oldUpdates = meter.updates;

    // Now try all possible instantiations until the solver is satisfied

    ExprMap successfulSubs;
    stack<ExprMap> freeSubs = MT::findInstantiationsForTempVars(its, meter.guard);

    while (!freeSubs.empty()) {
        const ExprMap &sub = freeSubs.top();

        //apply current substitution (and forget the previous one)
        meter.guard = oldGuard; // copy
        for (Expression &ex : meter.guard) ex.applySubs(sub);

        meter.updates = oldUpdates; // copy
        MT::applySubsToUpdates(sub, meter.updates);

        // Perform the first steps from generate() again (guard/update have changed)
        meter.simplifyAndFindVariables();
        meter.buildMeteringVariables();
        meter.buildLinearConstraints();

        solver->resetSolver();
        solver->add(meter.genNotGuardImplication());
        solver->add(meter.genUpdateImplications());
        solver->add(meter.genNonTrivial());
        solver->add(meter.genGuardPositiveImplication(false));
        smtRes = solver->check();

        if (smtRes == Smt::Sat) {
            successfulSubs = sub;
            break;
        }

        freeSubs.pop();
    }

    // If we found a successful instantiation, z3res is sat
    if (smtRes != Smt::Sat) {
        return {};
    }

    // Apply the successful instantiation to the entire rule
    Rule instantiatedRule = rule;
    instantiatedRule.applySubstitution(successfulSubs);

    // Proof output
    ProofOutput proof;
    proof.ruleTransformationProof(rule, "instantiation", instantiatedRule, its);

    return {{instantiatedRule, proof}};
}
