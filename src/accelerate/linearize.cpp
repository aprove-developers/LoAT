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

#include "linearize.h"

#include "expr/relation.h"

using namespace std;
using boost::optional;



bool Linearize::substituteExpression(const Expression &ex, string name) {
    ExprSymbolSet vars = ex.getVariables();

    // Check if the variables have already been substituted in a different way or are updated.
    // (it is not sound to substitute x^2 and x^3 by different, independent variables)
    for (const ExprSymbol &sym : vars) {
        if (subsVars.count(sym) > 0) return false;
        if (update.count(varMan.getVarIdx(sym)) > 0) return false;
    }

    VariableIdx freshVar = varMan.addFreshVariable(name);
    subsVars.insert(vars.begin(), vars.end());
    subsMap[ex] = varMan.getGinacSymbol(freshVar);

    return true;
};


bool Linearize::linearizeExpression(Expression &term) {
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
                    guard.push_back(subsMap[pow] >= 0);
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


bool Linearize::linearizeGuard() {
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
        if (!linearizeExpression(lhs) return false;
        if (!linearizeExpression(rhs) return false;

        term = Relation::replaceLhsRhs(term, lhs, rhs);
    }

    // Check if any of the substituted variables still occurs (e.g. x^2 substituted, but x > 4 appears)
    for (const Expression &term : guard) {
        for (const ExprSymbol &var : subsVars) {
            if (term.has(var)) {
                return false;
            }
        }
    }

    return true;
}


bool Linearize::linearizeUpdate() {
    for (auto it : update) {
        // first apply the current substitution
        it.second.applySubs(subsMap);

        // then try to linearize the update expression
        if (!linearizeExpression(it.second)) {
            return false;
        }
    }

    // Check if any of the substituted variables still occurs (e.g. x^2 substituted, but y := x + 4 appears)
    for (const auto &it : update) {
        for (const ExprSymbol &var : subsVars) {
            if (it.second.has(var)) {
                return false;
            }
        }
    }

    return true;
}


void Linearize::applySubstitution() {
    if (!subsMap.empty()) {
        for (Expression &term : guard) {
            term.applySubs(subsMap);
        }
        for (auto it : update) {
            it.second.applySubs(subsMap);
        }
    }
}


GiNaC::exmap Linearize::reverseSubstitution() const {
    // Calculate reverse substitution
    Substitution reverseSubs;
    for (auto it : subsMap) {
        reverseSubs[it.second] = it.first;
    }
    return reverseSubs;
}



optional<Substitution> Linearize::linearizeGuardUpdate(GuardList &guard, UpdateMap &update, VarMan &varMan) {
    Linearize lin(guard, update, varMan);

    if (!lin.linearizeGuard()) {
        return {};
    }

    if (!lin.linearizeUpdate()) {
        return {};
    }

    // Make sure that the resulting substitution is applied everywhere.
    // (for the current implementation, this is probably not necessary).
    lin.applySubstitution();

    return lin.reverseSubstitution();
}
