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

#include "metertools.hpp"

#include "../../expr/guardtoolbox.hpp"
#include "../../expr/relation.hpp"
#include "../../smt/smt.hpp"
#include "../../smt/smtfactory.hpp"
#include "../../expr/boolexpr.hpp"
#include "../../config.hpp"

using namespace std;


/* ### Helpers ### */

void MeteringToolbox::applySubsToUpdates(const ExprMap &subs, MultiUpdate &updates) {
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

    // Collect all updated variables (updated by any of the updates)
    ExprSymbolSet updatedVars;
    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            updatedVars.insert(varMan.getVarSymbol(it.first));
        }
    }
    auto isUpdated = [&](const ExprSymbol &var){ return updatedVars.count(var) > 0; };

    // create Z3 solver with the guard here to use push/pop for efficiency
    unique_ptr<Smt> solver = SmtFactory::solver(Smt::chooseLogic({guard}, updates), varMan);
    for (const Expression &ex : guard) {
        solver->add(ex);
    }

    for (const Expression &ex : guard) {
        // only keep constraints that contain updated variables (otherwise they still hold after the update)
        bool add = ex.hasVariableWith(isUpdated);

        // and only if they are not implied after each update (so they may cause the loop to terminate)
        if (add) {
            bool implied = true;
            for (const UpdateMap &update : updates) {
                solver->push();
                solver->add(!buildLit(ex.subs(update.toSubstitution(varMan))));
                auto smtRes = solver->check();
                solver->pop();

                // unsat means that the updated `ex` must always hold (i.e., is implied after the update)
                if (smtRes != Smt::Unsat) {
                    implied = false;
                    break;
                }
            }

            if (implied) {
                add = false;
            }
        }

        // add the constraint, or remember it as being irrelevant
        if (add) {
            reducedGuard.push_back(ex);
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
    bool changed = false;

    // first remove irrelevant constraints from the guard
    GuardList reducedGuard = reduceGuard(varMan, guard, updates);
    set<VariableIdx> relevantVars = findRelevantVariables(varMan, reducedGuard, updates);

    // consider each update independently of the others
    for (const UpdateMap &update : updates) {
        // helper lambda to pass as argument
        auto isUpdated = [&](const ExprSymbol &sym){ return update.isUpdated(varMan.getVarIdx(sym)); };
        ExprMap updateSubs = update.toSubstitution(varMan);

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
            ExprSymbol lhsVar = varMan.getVarSymbol(it.first);

            ExprMap subs;
            subs[varMan.getVarSymbol(it.first)] = it.second;

            for (const Expression &ex : reducedGuard) {
                if (ex.has(lhsVar)) {

                    // We want to make sure that all constraints with lhsVar hold after the update.
                    // E.g. if x := 4, y := y+1 and the guard is x > y, we add 4 > y+1.
                    // Note that only updating x (i.e., adding 4 > y) might not be sufficient.
                    Expression add = ex.subs(updateSubs);

                    // Adding trivial constraints does not make sense (no matter if they are true/false).
                    bool isTrivial = Relation::checkTrivial(add).is_initialized();
                    if (!isTrivial) {
                        guard.push_back(add);
                        changed = true;
                    }
                }
            }
        }
    }
    return changed;
}


stack<ExprMap> MeteringToolbox::findInstantiationsForTempVars(const VarMan &varMan, const GuardList &guard) {
    namespace Config = Config::ForwardAccel;
    assert(Config::TempVarInstantiationMaxBounds > 0);

    //find free variables
    const set<VariableIdx> &freeVar = varMan.getTempVars();
    if (freeVar.empty()) return stack<ExprMap>();

    //find all bounds for every free variable
    map<VariableIdx,ExpressionSet> freeBounds;
    for (const Expression &ex : guard) {
        for (VariableIdx freeIdx : freeVar) {
            auto it = freeBounds.find(freeIdx);
            if (it != freeBounds.end() && it->second.size() >= Config::TempVarInstantiationMaxBounds) continue;

            ExprSymbol free = varMan.getVarSymbol(freeIdx);
            if (!ex.has(free)) continue;

            Expression term = Relation::toLessEq(ex);
            auto optSolved = GuardToolbox::solveTermFor(term.lhs()-term.rhs(), free, GuardToolbox::ResultMapsToInt);
            if (!optSolved) continue;

            freeBounds[freeIdx].insert(optSolved.get());
        }
    }

    //check if there are any bounds at all
    if (freeBounds.empty()) return stack<ExprMap>();

    //combine all bounds in all possible ways
    stack<ExprMap> allSubs;
    allSubs.push(ExprMap());
    for (auto const &it : freeBounds) {
        ExprSymbol sym = varMan.getVarSymbol(it.first);
        for (const Expression &bound : it.second) {
            stack<ExprMap> next;
            while (!allSubs.empty()) {
                ExprMap subs = allSubs.top();
                allSubs.pop();
                if (subs.contains(sym)) {
                    //keep old bound, add a new substitution for the new bound
                    ExprMap newsubs = subs;
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
