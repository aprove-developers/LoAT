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
#include "metertools.h"
#include "debug.h"

using namespace std;
using boost::optional;


bool Linearize::collectNonlinearTerms(const Expression &ex, ExpressionSet &nonlinearTerms) {
    // We can only handle polynomials
    if (!ex.isPolynomial()) {
        debugLinearize("Too complicated, not polynomial: " << ex);
        return false;
    }

    // Check if we are linear in every variable
    for (const ExprSymbol &var : ex.getVariables()) {
        int deg = ex.degree(var);
        assert(deg >= 0); // since ex is a polynomial

        // If x occurs with different degrees, we cannot substitute both (e.g. x^2 + x)
        if (deg > 1) {
            // remove the absolute coeff to exclude degree 0 when calling ldegree
            Expression shifted = (ex - ex.coeff(var,0)).expand(); // expand appears to be needed for ldegree
            int lowdeg = shifted.ldegree(var);
            assert(lowdeg > 0);

            if (lowdeg != deg) {
                debugLinearize("Too complicated, " << var << " appears with different degrees: " << ex);
                return false;
            }
        }

        if (deg > 1) {
            // Substitute powers of x, e.g. 4*x^2 should later become 4*z.
            // We don't handle cases like y*x^2 to keep linearization simple.
            if (!ex.coeff(var, deg).info(GiNaC::info_flags::numeric)) {
                debugLinearize("Too complicated, " << var << " has power with non-constant coeff: " << ex);
                return false; // too complicated
            }
            nonlinearTerms.insert(GiNaC::pow(var, deg));

        } else {
            // If deg == 1, we can still have a nonlinear term like x*y, so we have to check the coefficient.
            // We don't handle more complicated cases like x*y*z or (y+z)*x.
            Expression coeff = ex.coeff(var, deg);
            ExprSymbolSet coeffVars = coeff.getVariables();

            if (coeffVars.size() > 1) {
                debugLinearize("Too complicated, " << var << " has coeff with mutliple variables: " << ex);
                return false; // too complicated
            }

            if (coeffVars.size() == 1) {
                ExprSymbol coeffVar = *coeffVars.begin();
                nonlinearTerms.insert(coeffVar * var);
            }
        }
    }
    return true;
}


bool Linearize::collectNonlinearTermsInGuard(ExpressionSet &nonlinearTerms) const {
    for (const Expression &ex : guard) {
        assert(Relation::isInequality(ex));

        if (!collectNonlinearTerms(ex.lhs(), nonlinearTerms)) {
            return false;
        }

        if (!collectNonlinearTerms(ex.rhs(), nonlinearTerms)) {
            return false;
        }
    }
    return true;
}


bool Linearize::collectNonlinearTermsInUpdates(ExpressionSet &nonlinearTerms) const {
    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            if (!collectNonlinearTerms(it.second, nonlinearTerms)) {
                return false;
            }
        }
    }
    return true;
}


bool Linearize::checkForConflicts(const ExpressionSet &nonlinearTerms) const {
    ExprSymbolSet vars;
    for (const Expression &term : nonlinearTerms) {
        for (ExprSymbol var : term.getVariables()) {
            // If we already know this variable, we have a conflict,
            // since we cannot replace a variable in two different ways.
            if (vars.count(var) > 0) {
                return false;
            }

            // If the variable is updated, we cannot replace it
            if (MeteringToolbox::isUpdatedByAny(varMan.getVarIdx(var), updates)) {
                return false;
            }

            // Otherwise the replacement is safe
            vars.insert(var);
        }
    }
    return true;
}


GiNaC::exmap Linearize::buildSubstitution(const ExpressionSet &nonlinearTerms) {
    using namespace GiNaC;

    // FIXME: If we substitute a free variable, the result should be free as well?!
    // FIXME: This *might* be soundness critical!
    // FIXME: Alternative: Never substitute fresh variables (and check if this affects the benchmarks)

    GiNaC::exmap res;
    for (const Expression &term: nonlinearTerms) {
        if (is_a<power>(term)) {
            symbol base = ex_to<symbol>(term.op(0));
            int exponent = ex_to<numeric>(term.op(1)).to_int();

            VariableIdx freshIdx = varMan.addFreshVariable(base.get_name() + to_string(exponent));
            ExprSymbol fresh = varMan.getGinacSymbol(freshIdx);
            res[term] = fresh;

            // Remember that e.g. x^2 is always nonnegative
            if (exponent % 2 == 0) {
                additionalGuard.push_back(fresh >= 0);
            }

        } else {
            assert(term.nops() == 2 && is_a<mul>(term)); // form x*y
            symbol x = ex_to<symbol>(term.op(0));
            symbol y = ex_to<symbol>(term.op(1));

            VariableIdx freshIdx = varMan.addFreshVariable(x.get_name() + y.get_name());
            ExprSymbol fresh = varMan.getGinacSymbol(freshIdx);
            res[term] = fresh;
        }
    }
    return res;
}


void Linearize::applySubstitution(const GiNaC::exmap &subs) {
    // We have to enable algebraic substitutions, as otherwise x*y*z stays x*y*z
    // if we apply the exmap x*y/xy (since x*y only matches a part of the GiNaC::mul).
    // See the GiNaC documentation/tutorial on subs() for more details.
    auto subsOptions = GiNaC::subs_options::algebraic;

    for (Expression &term : guard) {
        term = term.subs(subs, subsOptions);
    }

    for (UpdateMap &update : updates) {
        for (auto &it : update) {
            it.second = it.second.subs(subs, subsOptions);
        }
    }
}


GiNaC::exmap Linearize::reverseSubstitution(const GiNaC::exmap &subs) {
    GiNaC::exmap reverseSubs;
    for (auto it : subs) {
        assert(it.second.info(GiNaC::info_flags::symbol));
        reverseSubs[it.second] = it.first;
    }
    return reverseSubs;
}


optional<GiNaC::exmap> Linearize::linearizeGuardUpdates(VarMan &varMan, GuardList &guard, std::vector<UpdateMap> &updates) {
    debugLinearize("Trying to linearize the following guard/updates:");
    dumpGuardUpdates("linearize", guard, updates);
    Linearize lin(guard, updates, varMan);

    // Collect all nonlinear terms that have to be replaced (if possible)
    ExpressionSet nonlinearTerms;
    if (!lin.collectNonlinearTermsInGuard(nonlinearTerms)) {
        debugLinearize("Cannot linearize, guard too complicated");
        return {};
    }
    if (!lin.collectNonlinearTermsInUpdates(nonlinearTerms)) {
        debugLinearize("Cannot linearize, guard too complicated");
        return {};
    }

    // If everything is linear, there is nothing to do
    if (nonlinearTerms.empty()) {
        debugLinearize("Everything is linear, nothing to do");
        return GiNaC::exmap(); // empty substitution
    }

    // Check if it is safe to replace all nonlinear terms
    if (!lin.checkForConflicts(nonlinearTerms)) {
        debugLinearize("Cannot linearize due to conflicts");
        return {};
    }

    // Construct the replacement and apply it
    GiNaC::exmap subs = lin.buildSubstitution(nonlinearTerms);
    lin.applySubstitution(subs);

    // Check that everything is now indeed linear (can be removed, just to check the current implementation)
    // FIXME: Remove this if it is never hit
    for (const Expression &ex : guard) {
        assert(Expression(ex.lhs()).isLinear());
        assert(Expression(ex.rhs()).isLinear());
    }
    for (const UpdateMap &update : updates) {
        for (const auto &it : update) {
            assert(it.second.isLinear());
        }
    }

    // Add the additional guard (to retain the information that e.g. x^2 is nonnegative)
    for (Expression ex : lin.additionalGuard) {
        guard.push_back(ex);
    }

    debugLinearize("Applied linearization: " << subs);
    return lin.reverseSubstitution(subs);
}
