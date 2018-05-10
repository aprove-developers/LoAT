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

#include "nl_linearize.h"

#include "expr/relation.h"
#include "nl_metertools.h"

using namespace std;
using boost::optional;


bool LinearizeNL::substituteExpression(const Expression &ex, string name) {
    ExprSymbolSet vars = ex.getVariables();

    // Check if the variables have already been substituted in a different way or are updated.
    // (it is not sound to substitute x^2 and x^3 by different, independent variables)
    for (const ExprSymbol &sym : vars) {
        if (subsVars.count(sym) > 0) return false;
        if (MeteringToolboxNL::isUpdatedByAny(varMan.getVarIdx(sym), updates)) return false;
    }

    // FIXME: If we substitute a free variable, the result should be free as well?!
    // FIXME: This *might* be soundness critical!
    // FIXME: Alternative: Never substitute fresh variables (and check if this affects the benchmarks)
    VariableIdx freshVar = varMan.addFreshVariable(name);
    subsVars.insert(vars.begin(), vars.end());
    subsMap[ex] = varMan.getGinacSymbol(freshVar);

    return true;
};


bool LinearizeNL::linearizeExpression(Expression &term) {
    // term must be a polynomial ...
    ExprSymbolSet vars = term.getVariables();
    if (!term.isPolynomialWithin(vars)) {
        return false;
    }

    // ... and linear in every variable
    for (const ExprSymbol &var : vars) {

        for (;;) {
            int deg = term.degree(var);
            assert(deg >= 0); // we only consider polynomials

            // substitute powers, e.g. x^2 --> "x2"
            if (deg > 1) {
                Expression pow = GiNaC::pow(var,deg);
                if (!substituteExpression(pow, var.get_name()+to_string(deg))) {
                    return false;
                }

                // apply the substitution (so degree changes in the next iteration)
                term.applySubs(subsMap);

                //squared variables are always non-negative, keep this information
                if (deg % 2 == 0) {
                    additionalGuard.push_back(subsMap.at(pow) >= 0);
                }
            }
                // heuristic to substitute simple variable products, e.g. x*y --> "xy"
            else if (deg == 1) {
                GiNaC::ex coeff = term.coeff(var,1);
                if (GiNaC::is_a<GiNaC::numeric>(coeff)) break; // linear occurrences are ok

                // give up on complicated cases like x*y*z
                ExprSymbolSet syms = Expression(coeff).getVariables();
                if (syms.size() > 1) {
                    debugFarkas("Nonlinear substitution: too complex for simple heuristic");
                    return false;
                }

                assert(!syms.empty()); //otherwise coeff would be numeric above
                ExprSymbol var2 = *syms.begin();
                if (!substituteExpression(var*var2, var.get_name()+var2.get_name())) {
                    return false;
                }

                // apply the substitution (so degree changes in the next iteration)
                term.applySubs(subsMap);
            }
            else {
                break; // we substituted all occurrences
            }
        }
    }
    return true;
}


bool LinearizeNL::linearizeGuard() {
    // Collect all variables from the guard
    ExprSymbolSet vars;
    for (const Expression &term : guard) {
        term.collectVariables(vars);
    }

    // Linearize guard
    for (Expression &term : guard) {
        assert(Relation::isInequality(term));

        // first apply the current substitution
        Expression lhs = term.lhs().subs(subsMap);
        Expression rhs = term.rhs().subs(subsMap);

        // then try to linearize lhs and rhs (by enlarging the substitution, if possible)
        if (!linearizeExpression(lhs)) return false;
        if (!linearizeExpression(rhs)) return false;

        term = Relation::replaceLhsRhs(term, lhs, rhs);
    }

    return true;
}


bool LinearizeNL::linearizeUpdates() {
    for (UpdateMap &update : updates) {
        for (auto &it : update) {
            // first apply the current substitution
            it.second.applySubs(subsMap);

            // then try to linearize the update expression
            if (!linearizeExpression(it.second)) {
                return false;
            }
        }
    }

    return true;
}


bool LinearizeNL::checkForConflicts() const {
    // Check guard (e.g. x^2 substituted, but x > 4 appeared before, so we did not notice)
    for (const Expression &term : guard) {
        for (const ExprSymbol &var : subsVars) {
            if (term.has(var)) {
                return false;
            }
        }
    }

    // Check the updates (e.g. if y := x^2 was substituted, but we also have the guard x < 4)
    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            for (const ExprSymbol &var : subsVars) {
                if (it.second.has(var)) {
                    return false;
                }
            }
        }
    }

    return true;
}


void LinearizeNL::applySubstitution() {
    if (!subsMap.empty()) {
        for (Expression &term : guard) {
            term.applySubs(subsMap);
        }
        for (UpdateMap &update : updates) {
            for (auto &it : update) {
                it.second.applySubs(subsMap);
            }
        }
    }
}


GuardList LinearizeNL::getAdditionalGuard() const {
    return additionalGuard;
}


GiNaC::exmap LinearizeNL::reverseSubstitution() const {
    // Calculate reverse substitution
    GiNaC::exmap reverseSubs;
    for (auto it : subsMap) {
        reverseSubs[it.second] = it.first;
    }
    return reverseSubs;
}



optional<GiNaC::exmap> LinearizeNL::linearizeGuardUpdates(VarMan &varMan, GuardList &guard, std::vector<UpdateMap> &updates) {
    LinearizeNL lin(guard, updates, varMan);

    if (!lin.linearizeGuard()) {
        return {};
    }

    if (!lin.linearizeUpdates()) {
        return {};
    }

    if (!lin.checkForConflicts()) {
        return {};
    }

    // Make sure that the resulting substitution is applied everywhere.
    // (for the current implementation, this is probably not necessary).
    lin.applySubstitution();

    // Add the additional guard (to retain the information that x^2 is nonnegative)
    for (Expression ex : lin.getAdditionalGuard()) {
        guard.push_back(ex);
    }

    return lin.reverseSubstitution();
}
