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

#include "farkas.h"

#include "timing.h"
#include "guardtoolbox.h"
#include "z3toolbox.h"
#include "expression.h"
#include "flowgraph.h"
#include "timeout.h"

#include <string>
#include <map>
#include <vector>
#include <list>
#include <stack>

using namespace std;


FarkasMeterGenerator::FarkasMeterGenerator(ITRSProblem &itrs, const Transition &t)
    : itrs(itrs), update(t.update), guard(t.guard),
      coeff0(context.getFreshVariable("c",Z3VariableContext::Real))
{}


/* ### Preprocessing to eliminate free var ### */

void FarkasMeterGenerator::preprocessFreevars() {
    //equalities might be helpful to remove free variables
    GuardToolbox::findEqualities(guard);

    //precalculate relevant variables (probably just an estimate) to improve free var elimination
    reduceGuard();
    findRelevantVariables();

    //find all variables on update rhs (where lhs is a relevant variable)
    ExprSymbolSet varsInUpdate;
    for (auto it : update) if (isRelevantVariable(it.first)) it.second.collectVariables(varsInUpdate);

    //helpers for guard preprocessing
    auto sym_is_free = [&](const ExprSymbol &sym){ return itrs.isFreeVar(itrs.getVarindex(sym.get_name())); };
    auto free_in_update = [&](const ExprSymbol &sym){ return sym_is_free(sym) && varsInUpdate.count(sym) > 0; };
    auto free_noupdate = [&](const ExprSymbol &sym){ return sym_is_free(sym) && varsInUpdate.count(sym) == 0; };

    //try to remove free variables from the update rhs first
    GiNaC::exmap equalSubs;
    GuardToolbox::propagateEqualities(itrs,guard,GuardToolbox::NoCoefficients,GuardToolbox::NoFreeOnRhs,&equalSubs,free_in_update);
    debugFarkas("FARKAS: propagating the equalities " << equalSubs);
    for (auto &it : update) it.second = it.second.subs(equalSubs);

#ifdef DEBUG_FARKAS
    dumpGuardUpdate("Update propagation",guard,update);
#endif

    //try to remove free variables from equalities
    equalSubs.clear();
    GuardToolbox::propagateEqualities(itrs,guard,GuardToolbox::NoCoefficients,GuardToolbox::NoFreeOnRhs,&equalSubs,sym_is_free);
    for (auto &it : update) it.second = it.second.subs(equalSubs);

#ifdef DEBUG_FARKAS
    dumpGuardUpdate("Propagated Guard",guard,update);
#endif

    //now eliminate a <= x and replace a <= x, x <= b by a <= b for all free variables x where this is sound
    GuardToolbox::eliminateByTransitiveClosure(guard,itrs.getGinacVarList(),true,free_noupdate);

#ifdef DEBUG_FARKAS
    dumpList("Transitive Elimination Guard",guard);
#endif

    //clear precalculated data (not neccessary, but safer to understand)
    reducedGuard.clear();
    varlist.clear();
}


/* ### Filter relevant constarints/variables ### */


bool FarkasMeterGenerator::makeRelationalGuard() {
    vector<Expression> newGuard;
    for (Expression term : guard) {
        if (term.info(GiNaC::info_flags::relation_not_equal)) return false; //not allowed
        if (term.info(GiNaC::info_flags::relation_equal)) {
            newGuard.push_back(term.lhs() <= term.rhs());
            newGuard.push_back(term.lhs() >= term.rhs());
        } else {
            newGuard.push_back(term);
        }
    }
    guard = std::move(newGuard);
    return true;
}


void FarkasMeterGenerator::reduceGuard() {
    reducedGuard.clear();
    irrelevantGuard.clear();

    //create Z3 solver here to use push/pop for efficiency
    Z3VariableContext c;
    Z3Solver sol(c);
    z3::params params(c);
    params.set(":timeout", Z3_CHECK_TIMEOUT);
    sol.set(params);
    for (const Expression &ex : guard) sol.add(ex.toZ3(c));

    for (Expression ex : guard) {
        bool add = false;
        bool add_always = false;
        GiNaC::exmap updateSubs;
        auto varnames = ex.getVariableNames();
        for (string varname : varnames) {
            VariableIndex vi = itrs.getVarindex(varname);
            //keep constraint if it contains a free variable
            if (itrs.isFreeVar(vi)) {
                add_always = true;
            }
            //keep constraint if it contains an updated variable
            auto upIt = update.find(vi);
            if (upIt != update.end()) {
                add = true;
                updateSubs[itrs.getGinacSymbol(upIt->first)] = upIt->second;
            }
        }
        //add if ex contains free var OR updated variable and is not trivially true for the update
        if (add_always) {
            reducedGuard.push_back(ex);
        } else if (!add) {
            irrelevantGuard.push_back(ex);
        } else {
            sol.push();
            sol.add(!Expression::ginacToZ3(ex.subs(updateSubs),c));
            bool tautology = (sol.check() == z3::unsat);
            if (tautology) {
                irrelevantGuard.push_back(ex);
            } else {
                reducedGuard.push_back(ex);
            }
            sol.pop();
        }
    }
}


void FarkasMeterGenerator::findRelevantVariables() {
    varlist.clear();
    set<VariableIndex> added;

    //helper to add a variable based on its name, returns true iff the variable did not yet exist
    auto addVar = [&](const string &name) -> bool {
        VariableIndex idx = itrs.getVarindex(name);
        if (added.find(idx) != added.end()) return false;
        added.insert(idx);
        varlist.push_back(idx);
        return true;
    };

    //calculate relevant variables in the reduced guard and rhs update
    set<string> guardVarnames;
    for (const Expression &e : reducedGuard) {
        e.collectVariableNames(guardVarnames);
    }
    for (const string &name : guardVarnames) {
        addVar(name);
    }

    //iteratively find all relevant variables
    bool changed;
    do {
        changed = false;
        for (auto up : update) {
            if (added.count(up.first) == 0) continue; //ignore for this iteration
            auto names = up.second.getVariableNames();
            for (const string &name : names) {
                changed = addVar(name) || changed;
            }
        }
    } while (changed);

    //add the corresponding ginac symbols
    symbols.clear();
    for (VariableIndex vi : varlist) {
        symbols.push_back(itrs.getGinacSymbol(vi));
    }
}


bool FarkasMeterGenerator::isRelevantVariable(VariableIndex vi) const {
    return std::find(varlist.begin(),varlist.end(),vi) != varlist.end();
}


void FarkasMeterGenerator::restrictToRelevantVariables() {
    auto contains_relevant = [&](const Expression &ex) {
        auto names = ex.getVariableNames();
        return std::any_of(names.begin(),names.end(),[&](const string &s){ return isRelevantVariable(itrs.getVarindex(s)); });
    };

    //remove updates of irrelevant variables
    set<VariableIndex> toRemove;
    for (auto it : update) {
        if (!isRelevantVariable(it.first)) toRemove.insert(it.first);
    }
    for (VariableIndex vi : toRemove) {
        update.erase(vi);
    }

    //remove guards not containing relevant variables
    for (int i=0; i < guard.size(); ++i) {
        if (!contains_relevant(guard[i])) {
            guard.erase(guard.begin()+i);
            i--;
        }
    }

    //irrelevantGuard might contain terms that should be removed
    for (int i=0; i < irrelevantGuard.size(); ++i) {
        if (!contains_relevant(irrelevantGuard[i])) {
            irrelevantGuard.erase(irrelevantGuard.begin()+i);
            i--;
        }
    }

    //reducedGuard must not contain any terms that would be removed per definition
    for (int i=0; i < reducedGuard.size(); ++i) {
        assert(contains_relevant(reducedGuard[i]));
    }
}


void FarkasMeterGenerator::createCoefficients(Z3VariableContext::VariableType type) {
    coeffs.clear();
    for (int i=0; i < varlist.size(); ++i) {
        coeffs.push_back(context.getFreshVariable("c",type));
    }
}




/* ### Make guard/update linear by substitution ### */


bool FarkasMeterGenerator::makeLinear(Expression &term, ExprList vars, ExprSymbolSet &subsVars, GiNaC::exmap &subsMap) {
    //term must be a polynomial
    if (!term.is_polynomial(vars)) return false;

    auto addSubs = [&](ExprSymbol var, Expression ex, string name) -> bool {
        if (subsVars.count(var) > 0) return false;
        if (update.count(itrs.getVarindex(var.get_name())) > 0) return false;
        subsVars.insert(var);
        subsMap[ex] = itrs.getGinacSymbol(itrs.addFreshVariable(name));
        term = term.subs(subsMap);
        return true;
    };

    //and linear in every variable
    for (auto x : vars) {
        ExprSymbol var = Expression::toSymbol(x);

        for (;;) {
            int deg = term.degree(var);
            //substitute powers, e.g. x^2 --> "x2"
            if (deg > 1 || deg < 0) {
                Expression pow = GiNaC::pow(var,deg);
                if (!addSubs(var,pow,var.get_name()+to_string(deg))) return false;
                //squared variables are always non-negative, keep this information
                if (deg % 2 == 0) guard.push_back(subsMap[pow] >= 0);
            }
            //substitute simple variable products, e.g. x*y --> "xy"
            else if (deg == 1) {
                GiNaC::ex coeff = term.coeff(var,1);
                if (GiNaC::is_a<GiNaC::numeric>(coeff)) break;
                ExprSymbolSet syms = Expression(coeff).getVariables();
                if (syms.size() > 1) {
                    debugFarkas("Nonlinear substitution: too complex for simple heuristic");
                    return false;
                }
                assert(!syms.empty()); //otherwise coeff would be numeric above
                ExprSymbol var2 = *syms.begin();
                if (!addSubs(var,var*var2,var.get_name()+var2.get_name())) return false;
                //also forbid to replace the second variable in a different term
                subsVars.insert(var2);
            }
            else {
                break;
            }
        }
    }
    return true;
}

bool FarkasMeterGenerator::makeLinearTransition() {
    ExprSymbolSet subsVars;
    GiNaC::exmap subsMap;

    GiNaC::lst varlistLst;
    for (ExprSymbol s : symbols) varlistLst.append(s);

    //make guard linear
    for (int i=0; i < guard.size(); ++i) {
        Expression term = guard[i];
        //expect relational term with lhs and rhs
        if (!GiNaC::is_a<GiNaC::relational>(term) || term.nops() != 2) return false;
        //dont allow == or !=, only <,<=,>,>=
        if(term.info(GiNaC::info_flags::relation_equal) || term.info(GiNaC::info_flags::relation_not_equal)) return false;
        //make lhs and rhs linear if possible
        Expression lhs = term.lhs().subs(subsMap);
        if (!makeLinear(lhs,varlistLst,subsVars,subsMap)) return false;
        Expression rhs = term.rhs().subs(subsMap);
        if (!makeLinear(rhs,varlistLst,subsVars,subsMap)) return false;
        guard[i] = GuardToolbox::replaceLhsRhs(term,lhs,rhs);
    }

    //check if substituted variables occur in guard (i.e. x^2 substituted, but x > 4 appears)
    for (Expression term : guard) {
        for (Expression x : varlistLst) {
            ExprSymbol var = Expression::toSymbol(x);
            if ((term.lhs().degree(var) == 1 || term.rhs().degree(var) == 1) && subsVars.count(var) > 0) return false;
        }
    }

    //make updates linear
    for (auto it : update) {
        //check only relevantVars
        if (!isRelevantVariable(it.first)) continue;
        //substitute update expression if possible
        Expression term = it.second.subs(subsMap);
        if (!makeLinear(term,varlistLst,subsVars,subsMap)) return false;
        it.second = term;
    }

    //apply final substitution to all guards/updates
    if (!subsMap.empty()) {
        for (Expression &term : guard) term = term.subs(subsMap);
        for (auto it = update.begin(); it != update.end(); ++it) it->second = it->second.subs(subsMap);
    }

    //calculate reverse substitution
    for (auto it : subsMap) nonlinearSubs[it.second] = it.first;
    return true;
}



/* ### Transform guard/update to "linear term <= constant" relations for Farkas ### */


void FarkasMeterGenerator::buildConstraints() {
    constraints.guard.clear();
    constraints.guardUpdate.clear();
    constraints.irrelevantGuard.clear();
    constraints.reducedGuard.clear();

    auto make_constraint = [&](const GiNaC::ex &rel, vector<Expression> &vec) {
        using namespace GuardToolbox;
        assert(isLinearInequality(rel,itrs.getGinacVarList()));
        GiNaC::ex tmp = makeLessEqual(rel);
        tmp = splitVariablesAndConstants(tmp);
        if (!isTrivialInequality(tmp)) vec.push_back(tmp);
    };

    for (Expression ex : reducedGuard) {
        make_constraint(ex,constraints.reducedGuard);
    }
    for (Expression ex : irrelevantGuard) {
        make_constraint(ex,constraints.irrelevantGuard);
    }
    for (Expression ex : guard) {
        make_constraint(ex,constraints.guard);
        make_constraint(ex,constraints.guardUpdate);
    }
    for (auto it : update) {
        ExprSymbol primed;
        auto primeIt = primedSymbols.find(it.first);
        if (primeIt != primedSymbols.end()) {
            primed = primeIt->second;
        } else {
            primedSymbols[it.first] = (primed = itrs.getFreshSymbol(itrs.getVarname(it.first)+"'"));
        }
        make_constraint(primed <= it.second, constraints.guardUpdate);
        make_constraint(primed >= it.second, constraints.guardUpdate);
    }

#ifdef DEBUG_FARKAS
    dumpList("GUARD",constraints.guard);
    dumpList("REDUCED G",constraints.reducedGuard);
    dumpList("IRRELEVANT G",constraints.irrelevantGuard);
    dumpList("G+UPDATE",constraints.guardUpdate);
#endif
}




/* ### Apply Farkas Lemma to formulate the constraints for the metering function ### */


z3::expr FarkasMeterGenerator::applyFarkas(const vector<Expression> &constraints, const vector<ExprSymbol> &vars, const vector<z3::expr> &coeff, z3::expr c0, int delta, Z3VariableContext &context) {
    assert(vars.size() == coeff.size());

#ifdef DEBUG_FARKAS
    debugFarkas("FARKAS");
    dumpList(" cstrt",constraints);
    dumpList(" vars ",vars);
    dumpList(" coeff",coeff);
    debugFarkas(" delta: " << delta);
#endif

    vector<z3::expr> res;
    vector<z3::expr> lambda;
    //create lambda variables, add lambda >= 0
    for (int i=0; i < constraints.size(); ++i) {
        assert(constraints[i].info(GiNaC::info_flags::relation_less_or_equal));
        lambda.push_back(context.getFreshVariable("l",Z3VariableContext::Real));
        res.push_back(lambda[i] >= 0);
    }

    //create mapping from every variable to its coefficient
    map<ExprSymbol,z3::expr,GiNaC::ex_is_less> varToCoeff;
    for (int i=0; i < vars.size(); ++i) {
        varToCoeff.emplace(vars[i],coeff[i]);
    }

    //search for additional variables used in constraints that do not belong to the resulting metering function
    //NOTE: this is required for the representation A*x of the constraints
    ExprSymbolSet constraintSymbols;
    for (const Expression &ex : constraints) ex.collectVariables(constraintSymbols);
    for (const ExprSymbol &sym : constraintSymbols) {
        if (varToCoeff.find(sym) == varToCoeff.end()) {
            debugFarkas("FARKAS NOTE: Adding additional variable with 0 coefficient: " << sym);
            varToCoeff.emplace(sym,context.real_val(0));
        }
    }

    //lambda^T * A = c^T
    for (auto varIt : varToCoeff) {
        z3::expr lambdaA = context.int_val(0);
        for (int j=0; j < constraints.size(); ++j) {
            z3::expr add = lambda[j] * Expression::ginacToZ3(constraints[j].lhs().coeff(varIt.first),context);
            lambdaA = (j==0) ? add : lambdaA+add;
        }
        res.push_back(lambdaA == varIt.second);
    }

    //lambda^T * b + c0 <= delta
    z3::expr sum = c0;
    for (int i=0; i < constraints.size(); ++i) {
        sum = sum + lambda[i] * Expression(constraints[i].rhs()).toZ3(context);
    }
    res.push_back(sum <= delta);

#ifdef DEBUG_FARKAS
    debugFarkas(" result:");
    for (z3::expr x : res) debugFarkas(" - " << x);
    debugFarkas("---");
#endif

    return Z3Toolbox::concatExpressions(context,res,Z3Toolbox::ConcatAnd);
}


z3::expr FarkasMeterGenerator::genNotGuardImplication() const {
    vector<z3::expr> res;
    vector<Expression> lhs;
    for (Expression g : constraints.reducedGuard) {
        lhs.push_back(GuardToolbox::negateLessEqualInequality(g)); //the important negated constraint
        res.push_back(applyFarkas(lhs,symbols,coeffs,coeff0,0,context));
        lhs.pop_back();
    }
    return Z3Toolbox::concatExpressions(context,res,Z3Toolbox::ConcatAnd);
}


z3::expr FarkasMeterGenerator::genGuardPositiveImplication(bool strict) const {
    //G => f(x) > 0
    //f(x) > 0  ==  -f(x) < 0  ==  -f(x) <= -1
    vector<z3::expr> negCoeff;
    for (z3::expr coeff : coeffs) {
        negCoeff.push_back(-coeff);
    }
    return applyFarkas(constraints.guard,symbols,negCoeff,-coeff0,strict ? -1 : -0,context);
}


z3::expr FarkasMeterGenerator::genUpdateImplication() const {
    //f(x)-f(x')  =>  x-x' for all primed x only, others can be left out (for efficiency!)
    vector<ExprSymbol> var;
    vector<z3::expr> coeff;
    for (int i=0; i < varlist.size(); ++i) {
        auto primedIt = primedSymbols.find(varlist[i]);
        if (primedIt == primedSymbols.end()) continue; //only updated variables

        var.push_back(symbols[i]);      //x
        var.push_back(primedIt->second);//x'
        coeff.push_back( coeffs[i]);    //coeff for x
        coeff.push_back(-coeffs[i]);    //coeff for x', i.e. negative coeff for x
    }
    return applyFarkas(constraints.guardUpdate,var,coeff,context.real_val(0),1,context);
}


z3::expr FarkasMeterGenerator::genNonTrivial() const {
    vector<z3::expr> res;
    for (z3::expr c : coeffs) {
        res.push_back(c != 0);
    }
    return Z3Toolbox::concatExpressions(context,res,Z3Toolbox::ConcatOr);
}



/* ### Model interpretation and main function ### */


Expression FarkasMeterGenerator::buildResult(const z3::model &model) const {
    Expression result = Z3Toolbox::getRealFromModel(model,coeff0);
    for (int i=0; i < coeffs.size(); ++i) {
        result = result + Z3Toolbox::getRealFromModel(model,coeffs[i]) * symbols[i];
    }
    debugFarkas(result << " --> ");
    result = result.subs(nonlinearSubs);
    debugFarkas(result);
    return result;
}


stack<GiNaC::exmap> FarkasMeterGenerator::instantiateFreeVariables() const {
    if (FREEVAR_INSTANTIATE_MAXBOUNDS == 0) return stack<GiNaC::exmap>();

    //find free variables
    const set<VariableIndex> &freeVar = itrs.getFreeVars();
    if (freeVar.empty()) return stack<GiNaC::exmap>();

    //find all bounds for every free variable
    map<VariableIndex,ExpressionSet> freeBounds;
    for (int i=0; i < guard.size(); ++i) {
        const Expression &ex = guard[i];

        for (VariableIndex freeIdx : freeVar) {
            auto it = freeBounds.find(freeIdx);
            if (it != freeBounds.end() && it->second.size() >= FREEVAR_INSTANTIATE_MAXBOUNDS) continue;

            ExprSymbol free = itrs.getGinacSymbol(freeIdx);
            if (!ex.has(free)) continue;

            Expression term = GuardToolbox::makeLessEqual(ex);
            term = term.lhs()-term.rhs();
            if (!GuardToolbox::solveTermFor(term,free,GuardToolbox::NoCoefficients)) continue;

            freeBounds[freeIdx].insert(term);
        }
    }

    //check if there are any bounds at all
    if (freeBounds.empty()) return stack<GiNaC::exmap>();

    //combine all bounds in all possible ways
    stack<GiNaC::exmap> allSubs;
    allSubs.push(GiNaC::exmap());
    for (auto const &it : freeBounds) {
        ExprSymbol sym = itrs.getGinacSymbol(it.first);
        for (const Expression &bound : it.second) {
            stack<GiNaC::exmap> next;
            while (!allSubs.empty()) {
                GiNaC::exmap subs = allSubs.top();
                allSubs.pop();
                if (subs.count(sym) > 0) {
                    //keep old bound, add a new substitution for the new bound
                    GiNaC::exmap newsubs = subs;
                    newsubs[sym] = bound;
                    next.push(subs);
                    next.push(newsubs);
                } else {
                    subs[sym] = bound;
                    next.push(subs);
                }
            }
            allSubs.swap(next);
        }
    }
    return allSubs;
}


bool FarkasMeterGenerator::prepareGuard(ITRSProblem &itrs, Transition &t) {
    Timing::Scope timer1(Timing::FarkasTotal);
    Timing::Scope timer2(Timing::FarkasLogic);
    bool changed = false;
    FarkasMeterGenerator f(itrs,t);
    f.reduceGuard();
    f.findRelevantVariables();
    for (const auto &it : f.update) {
        if (!f.isRelevantVariable(it.first)) continue;

        //check if the update rhs contains no updated variables
        bool skip = false;
        for (string varname : it.second.getVariableNames()) {
            if (f.update.find(itrs.getVarindex(varname)) != f.update.end()) {
                skip = true;
                break;
            }
        }

        //for every relevant constraint with it.first, replace this by the rhs
        if (!skip) {
            GiNaC::exmap guardSubs;
            guardSubs[itrs.getGinacSymbol(it.first)] = it.second;
            for (const Expression &ex : f.reducedGuard) {
                if (ex.has(itrs.getGinacSymbol(it.first))) {
                    t.guard.push_back(ex.subs(guardSubs));
                    changed = true;
                }
            }
        }
    }
    return changed;
}


FarkasMeterGenerator::Result FarkasMeterGenerator::generate(ITRSProblem &itrs, Transition &t, Expression &result, pair<VariableIndex, VariableIndex> *conflictVar) {
    Timing::Scope timer(Timing::FarkasTotal);
    Timing::start(Timing::FarkasLogic);
    FarkasMeterGenerator f(itrs,t);
    debugFarkas("FARKAS Transition:" << t);

    //preprocessing
    f.preprocessFreevars();
    if (!f.makeRelationalGuard()) {
        debugProblem("Farkas aborting on != for: " << t);
        Timing::done(Timing::FarkasLogic);
        return Nonlinear; // != is not allowed
    }

    //simplify guard
    f.reduceGuard();
    f.findRelevantVariables();
    f.restrictToRelevantVariables();

    //ensure linearity
    if (!f.makeLinearTransition()) {
        debugProblem("Farkas aborting on nonlinearity for: " << t);
        Timing::done(Timing::FarkasLogic);
        return Nonlinear;
    }
    if (!f.nonlinearSubs.empty()) {
#ifdef DEBUG_FARKAS
        debugFarkas("Nonlinear substitution map:" << endl << f.nonlinearSubs);
        dumpList("Resulting guard",f.guard);
        dumpMap("Resulting update",f.update);
#endif

        //recalculate reduced guard and relevant variables (probably changed by substitution)
        f.reduceGuard();
        f.findRelevantVariables();
        f.restrictToRelevantVariables();
    }

    if (f.reducedGuard.empty()) { //no guard is limiting the iteration, loop forever
        Timing::done(Timing::FarkasLogic);
        return Unbounded;
    }

    //transform constraints
    //if there are integer coefficients, we will still get them due to the f(x) >= 1 constraint, so don't waste time searching twice
#ifdef FARKAS_ALLOW_REAL_COEFFS
    Z3VariableContext::VariableType coeffType = Z3VariableContext::Real;
#else
    Z3VariableContext::VariableType coeffType = Z3VariableContext::Integer;
#endif
    f.buildConstraints();
    f.createCoefficients(coeffType);
    Timing::done(Timing::FarkasLogic);

    //solve implications
    Z3Solver solver(f.context);
    solver.add(f.genNotGuardImplication());
    solver.add(f.genUpdateImplication());
    solver.add(f.genNonTrivial());
    z3::check_result res = solver.check();

    //try to apply instantiation
    GiNaC::exmap replaceFreeSub;
    if (res == z3::unsat) {
        stack<GiNaC::exmap> freeSubs = f.instantiateFreeVariables();
        GuardList oldGuard = f.guard;
        UpdateMap oldUpdate = f.update;
        while (!freeSubs.empty()) {
            if (Timeout::soft()) break;

            const GiNaC::exmap &sub = freeSubs.top();
            debugFarkas("Trying instantiation: " << sub);
            Timing::start(Timing::FarkasLogic);

            //apply substitution
            f.guard.clear();
            for (Expression &ex : oldGuard) f.guard.push_back(ex.subs(sub));
            f.update.clear();
            for (const auto &up : oldUpdate) f.update[up.first] = up.second.subs(sub);

            //update information about variables and symbols, as the guard/update has changed!
            f.reduceGuard();
            f.findRelevantVariables();
            f.restrictToRelevantVariables();

            //transform constraints
            f.buildConstraints();
            f.createCoefficients(coeffType);
            Timing::done(Timing::FarkasLogic);

            //solve implications
            solver.reset();
            solver.add(f.genNotGuardImplication());
            solver.add(f.genUpdateImplication());
            solver.add(f.genNonTrivial());
            res = solver.check();
            if (res == z3::sat) {
                replaceFreeSub = sub;
                break;
            }

            freeSubs.pop();
        }
    }

    if (res == z3::unsat) {
        debugFarkas("z3 pre unsat");
        debugProblem("Farkas pre unsat for: " << t);

#ifdef FARKAS_HEURISTIC_FOR_MINMAX
        if (conflictVar) {
            //check if the problem is of the form: A++,B++ [ A < X, B < Y ] where we would require min(A,B) or max(A,B)
            //if this is the case, we set conflictVar, so the edge can be modified by adding A > B or B > A and Farkas can be tried again
            vector<VariableIndex> failVars;
            for (const auto &it : f.update) {
                auto rhsVars = it.second.getVariableNames();
                //the update must be some sort of simple counting, e.g. A = A+2
                if (rhsVars.size() != 1 || rhsVars.count(itrs.getVarname(it.first)) == 0) continue;
                //and there must be a guard term limiting the execution of this counting
                for (const Expression &x : f.reducedGuard) {
                    if (x.has(itrs.getGinacSymbol(it.first))) {
                        failVars.push_back(it.first);
                        break;
                    }
                }
            }
            //if we have more than 2 variables, there are too many possiblities, limit the heuristic to A > B and B > A.
            if (failVars.size() == 2) {
                debugProblem("Farkas found conflicting variables: " << itrs.getVarname(failVars[0]) << " and " << itrs.getVarname(failVars[1]));
                *conflictVar = make_pair(failVars[0],failVars[1]);
                return ConflictVar;
            }
        }
#endif

        return Unsat;
    }

    //first try the strictly positive implication, i.e. G => f(x) > 0 (i.e. f(x) >= 1).
    solver.push();
    solver.add(f.genGuardPositiveImplication(true));
    res = solver.check();

    //try the relaxed implication G => f(x) >= 0 as fallback
    if (res != z3::sat) {
        debugFarkas("z3 strict positive: " << res);
        debugProblem("Farkas strict positive is " << res << " for: " << t);
        solver.pop(); //remove last assertion
        solver.add(f.genGuardPositiveImplication(false));
        res = solver.check();
    }

    debugFarkas("z3 final res: " << res);

    if (res != z3::sat) {
        debugProblem("Farkas final result " << res << " for: " << t);
        return Unsat;
    }

    z3::model m = solver.get_model();
    debugFarkas("z3 model: " << m);

    //generate result from model and undo substitution
    result = f.buildResult(m);

    //in case of free var instantiation, apply the instantiation to the transition
    if (!replaceFreeSub.empty()) {
        for (Expression &ex : t.guard) ex = ex.subs(replaceFreeSub);
        for (auto &up : t.update) up.second = up.second.subs(replaceFreeSub);
        t.cost = t.cost.subs(replaceFreeSub);
    }

#ifdef FARKAS_ALLOW_REAL_COEFFS
    //check if there are real coefficients, then adjust metering function to ensure it is an integer
    function<int(int,int)> gcd;
    gcd = [&gcd](int a, int b) { return (b == 0) ? a : gcd(b, a % b); };
    auto lcm = [&gcd](int a, int b) { return (a*b) / gcd(a,b); };
    bool has_reals = false;
    int mult = 1;
    for (int i=0; i < f.coeffs.size(); ++i) {
        GiNaC::numeric c = GiNaC::ex_to<GiNaC::numeric>(Z3Toolbox::getRealFromModel(m,f.coeffs[i]));
        if (c.denom().to_int() != 1) {
            has_reals = true;
            mult = lcm(mult,c.denom().to_int());
        }
    }
    //remove reals from the metering function
    if (has_reals) {
        VariableIndex free = itrs.addFreshVariable("meter",true);
        t.guard.push_back(itrs.getGinacSymbol(free)*mult == result*mult);
        result = itrs.getGinacSymbol(free);
    }
#endif

    return Success;
}

