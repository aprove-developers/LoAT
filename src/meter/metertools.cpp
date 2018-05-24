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

#include "metertools.h"

#include "expr/guardtoolbox.h"
#include "expr/relation.h"
#include "z3/z3solver.h"
#include "z3/z3toolbox.h"

using namespace std;
using boost::optional;


/* ### Helpers ### */

void MeteringToolbox::applySubsToUpdates(const GiNaC::exmap &subs, MultiUpdate &updates) {
    for (UpdateMap &update : updates) {
        for (auto &it : update) {
            it.second.applySubs(subs);
        }
    }
}


bool MeteringToolbox::isUpdatedByAny(VariableIdx var, const MultiUpdate &updates) {
    auto updatesVar = [&](const UpdateMap &update) { return update.isUpdated(var); };
    return std::any_of(updates.begin(), updates.end(), updatesVar);
}


/* ### Preprocessing ### */

// FIXME: Return the applied substitution
void MeteringToolbox::eliminateTempVars(const VarMan &varMan, GuardList &guard, MultiUpdate &updates) {
    //equalities might be helpful to remove free variables
    GuardToolbox::findEqualities(guard);

    //precalculate relevant variables (probably just an estimate at this point) to improve free var elimination
    GuardList reducedGuard = reduceGuard(varMan, guard, updates);
    set<VariableIdx> relevantVars = findRelevantVariables(varMan, reducedGuard, updates);

    //collect all variables that appear in the rhs of the update of a relevant variable
    ExprSymbolSet varsInUpdate;
    for (UpdateMap &update : updates) {
        for (const auto &it : update) {
            if (relevantVars.count(it.first) > 0) {
                it.second.collectVariables(varsInUpdate);
            }
        }
    }

    //declare helper lambdas to be passed as arguments
    auto isTemp = [&](const ExprSymbol &sym){ return varMan.isTempVar(sym); };
    auto isTempInUpdate = [&](const ExprSymbol &sym){ return isTemp(sym) && varsInUpdate.count(sym) > 0; };
    auto isTempNoUpdate = [&](const ExprSymbol &sym){ return isTemp(sym) && varsInUpdate.count(sym) == 0; };

    //try to remove free variables from the update by equality propagation (they are removed from guard and update)
    GiNaC::exmap equalSubs;
    GuardToolbox::propagateEqualities(varMan, guard, GuardToolbox::NoCoefficients, GuardToolbox::NoFreeOnRhs, &equalSubs, isTempInUpdate);
    applySubsToUpdates(equalSubs, updates);

#ifdef DEBUG_FARKAS
    dumpGuardUpdates("Update propagation", guard, updates);
#endif

    //try to remove all free variables by equality propagation
    //(due to the above step, this should only affect the guard. We still update the update to be on the safe side)
    equalSubs.clear();
    GuardToolbox::propagateEqualities(varMan, guard, GuardToolbox::NoCoefficients, GuardToolbox::NoFreeOnRhs, &equalSubs, isTemp);
    applySubsToUpdates(equalSubs, updates);

#ifdef DEBUG_FARKAS
    dumpGuardUpdates("Propagated Guard", guard, updates);
#endif

    //now eliminate a <= x and replace a <= x, x <= b by a <= b for all free variables x where this is sound
    //(it is not sound for variables that appear in the update, since we need their value for the update)
    GuardToolbox::eliminateByTransitiveClosure(guard, varMan.getGinacVarList(), true, isTempNoUpdate);

#ifdef DEBUG_FARKAS
    dumpList("Transitive Elimination Guard", guard);
#endif
}


GuardList MeteringToolbox::replaceEqualities(const GuardList &guard) {
    GuardList newGuard;

    for (const Expression &ex : guard) {
        assert(Relation::isRelation(ex));

        if (Relation::isEquality(ex)) {
            newGuard.push_back(ex.lhs() <= ex.rhs());
            newGuard.push_back(ex.lhs() >= ex.rhs());
        } else {
            newGuard.push_back(ex);
        }
    }

    return newGuard;
}


/* ### Filter relevant constarints/variables ### */

GuardList MeteringToolbox::reduceGuard(const VarMan &varMan, const GuardList &guard, const MultiUpdate &updates,
                                       GuardList *irrelevantGuard)
{
    assert(!irrelevantGuard || irrelevantGuard->empty());
    GuardList reducedGuard;

    // create Z3 solver with the guard here to use push/pop for efficiency
    Z3Context context;
    Z3Solver solver(context);
    for (const Expression &ex : guard) {
        solver.add(ex.toZ3(context));
    }

    // TODO: Set timeout for the Z3 solver (see backwardaccel branch)

    for (Expression ex : guard) {
        bool add = false;
        bool forceAdd = false;

        // Check if the constraint is relevant and should be added
        for (const ExprSymbol &var : ex.getVariables()) {
            // always keep constraints that contain free variables
            if (varMan.isTempVar(var)) {
                forceAdd = true;
                break;
            }
            // keep constraints that contain updated variables
            if (isUpdatedByAny(varMan.getVarIdx(var), updates)) {
                add = true;
            }
        }

        if (forceAdd) {
            reducedGuard.push_back(ex);

        } else if (add) {
            // Only add constraints with updated variables if they are not implied by EACH update (individually)
            bool implied = true;
            for (const UpdateMap &update : updates) {
                solver.push();
                solver.add(!GinacToZ3::convert(ex.subs(update.toSubstitution(varMan)), context));
                auto z3res = solver.check();
                solver.pop();

                // unsat means that the updated `ex` must always hold (i.e., is implied after the update)
                if (z3res != z3::unsat) {
                    implied = false;
                    break;
                }
            }

            if (!implied) {
                reducedGuard.push_back(ex);
            } else {
                if (irrelevantGuard) irrelevantGuard->push_back(ex);
            }

        } else {
            if (irrelevantGuard) irrelevantGuard->push_back(ex);
        }
    }

    return reducedGuard;
}


set<VariableIdx> MeteringToolbox::findRelevantVariables(const VarMan &varMan, const GuardList &guard, const MultiUpdate &updates) {
    set<VariableIdx> res;

    // Add all variables appearing in the guard
    ExprSymbolSet guardVariables;
    for (const Expression &ex : guard) {
        ex.collectVariables(guardVariables);
    }
    for (const ExprSymbol &sym : guardVariables) {
        res.insert(varMan.getVarIdx(sym));
    }

    // Compute the closure of res under ALL updates
    // so if an updated variable is in res, also add all variables of the update's rhs
    set<VariableIdx> todo = res;
    while (!todo.empty()) {
        ExprSymbolSet next;

        for (VariableIdx var : todo) {
            for (const UpdateMap &update : updates) {
                auto it = update.find(var);
                if (it != update.end()) {
                    ExprSymbolSet rhsVars = it->second.getVariables();
                    next.insert(rhsVars.begin(), rhsVars.end());
                }
            }
        }

        todo.clear();
        for (const ExprSymbol &sym : next) {
            VariableIdx var = varMan.getVarIdx(sym);
            if (res.count(var) == 0) {
                todo.insert(var);
            }
        }

        // collect all variables from every iteration
        res.insert(todo.begin(), todo.end());
    }

    return res;
}


void MeteringToolbox::restrictUpdatesToVariables(MultiUpdate &updates, const set<VariableIdx> &vars) {
    for (UpdateMap &update : updates) {
        set<VariableIdx> toRemove;

        for (auto it : update) {
            if (vars.count(it.first) == 0) {
                toRemove.insert(it.first);
            }
        }

        for (VariableIdx var : toRemove) {
            update.erase(var);
        }
    }
}


void MeteringToolbox::restrictGuardToVariables(const VarMan &varMan, GuardList &guard, const set<VariableIdx> &vars) {
    auto isContainedInVars = [&](const ExprSymbol &sym) {
        return vars.count(varMan.getVarIdx(sym)) > 0;
    };

    auto containsNoVars = [&](const Expression &ex) {
        ExprSymbolSet syms = ex.getVariables();
        return !std::any_of(syms.begin(), syms.end(), isContainedInVars);
    };

    // remove constraints not containing relevant variables (vars)
    guard.erase(std::remove_if(guard.begin(), guard.end(), containsNoVars), guard.end());
}


/* ### Heuristics to improve metering results ### */

bool MeteringToolbox::strengthenGuard(const VarMan &varMan, GuardList &guard, const MultiUpdate &updates) {
    // TODO: timing (was FarkasLogic and FarkasTotal before)?
    bool changed = false;

    // first remove irrelevant constraints from the guard
    GuardList reducedGuard = reduceGuard(varMan, guard, updates);
    set<VariableIdx> relevantVars = findRelevantVariables(varMan, reducedGuard, updates);

    // consider each update independently of the others
    for (const UpdateMap &update : updates) {
        // helper lambda to pass as argument
        auto isUpdated = [&](const ExprSymbol &sym){ return update.isUpdated(varMan.getVarIdx(sym)); };

        for (const auto &it : update) {
            // only consider relevant variables
            if (relevantVars.count(it.first) == 0) continue;

            // only proceed if the update's rhs contains no updated variables
            ExprSymbolSet rhsVars = it.second.getVariables();
            if (std::any_of(rhsVars.begin(), rhsVars.end(), isUpdated)) continue;

            // for every constraint containing it.first,
            // add a new constraint with it.first replaced by the update's rhs
            // (e.g. if x := 4 and the guard is x > 0, we also add 4 > 0)
            // (this makes the guard stronger and might thus help to find a metering function)
            ExprSymbol lhsVar = varMan.getGinacSymbol(it.first);

            // TODO: It might be better to apply the entire update!
            // TODO: Consider FibonacciNew: loop(B,C) -> Com_2(loop(B-1,1), loop(B-2,1)) :|: B >= 2 && B >= C
            // TODO: Adding B >= 1 makes no sense, but adding B-2 >= 1 would help!
            // TODO: So for every term containing C, we should just apply the update!
            // TODO: This is *much* better, since then the constraint is trivially true after the update and thus not part of the reducedGuard!

            GiNaC::exmap subs;
            subs[varMan.getGinacSymbol(it.first)] = it.second; // TODO: Use Substitution::singleton;

            for (const Expression &ex : reducedGuard) {
                if (ex.has(lhsVar)) {

                    // TODO: This is the implementation corresponding to the above comment
                    // TODO: This helps for the FibonacciNew example!
                    guard.push_back(ex.subs(update.toSubstitution(varMan)));

                    // TODO: This is the old implementation, which is probably just stupid :P
                    //guard.push_back(ex.subs(subs));

                    changed = true;
                }
            }
        }
    }
    return changed;
}


stack<GiNaC::exmap> MeteringToolbox::findInstantiationsForTempVars(const VarMan &varMan, const GuardList &guard) {
    if (FREEVAR_INSTANTIATE_MAXBOUNDS == 0) return stack<GiNaC::exmap>();

    //find free variables
    const set<VariableIdx> &freeVar = varMan.getTempVars();
    if (freeVar.empty()) return stack<GiNaC::exmap>();

    //find all bounds for every free variable
    map<VariableIdx,ExpressionSet> freeBounds;
    for (const Expression &ex : guard) {
        for (VariableIdx freeIdx : freeVar) {
            auto it = freeBounds.find(freeIdx);
            if (it != freeBounds.end() && it->second.size() >= FREEVAR_INSTANTIATE_MAXBOUNDS) continue;

            ExprSymbol free = varMan.getGinacSymbol(freeIdx);
            if (!ex.has(free)) continue;

            Expression term = Relation::toLessEq(ex);
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
        ExprSymbol sym = varMan.getGinacSymbol(it.first);
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
