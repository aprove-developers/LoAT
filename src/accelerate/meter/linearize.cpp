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

#include "linearize.hpp"

#include "metertools.hpp"
#include "../../config.hpp"

using namespace std;


bool Linearize::collectMonomials(const Expr &ex, ExprSet &monomials) {
    // We can only handle polynomials
    if (!ex.isPoly()) {
        return false;
    }

    // Check if we are linear in every variable
    for (const Var &var : ex.vars()) {
        int deg = ex.degree(var);
        assert(deg >= 0); // since ex is a polynomial

        // If x occurs with different degrees, we cannot substitute both (e.g. x^2 + x)
        if (deg > 1) {
            // remove the absolute coeff to exclude degree 0 when calling ldegree
            Expr shifted = (ex - ex.coeff(var,0)).expand(); // expand appears to be needed for ldegree
            int lowdeg = shifted.ldegree(var);
            assert(lowdeg > 0);

            if (lowdeg != deg) {
                return false;
            }
        }

        if (deg > 1) {
            // Substitute powers of x, e.g. 4*x^2 should later become 4*z.
            // We don't handle cases like y*x^2 to keep linearization simple.
            if (!ex.coeff(var, deg).isRationalConstant()) {
                return false; // too complicated
            }
            monomials.insert(GiNaC::pow(var, deg));

        } else {
            // If deg == 1, we can still have a nonlinear term like x*y, so we have to check the coefficient.
            // We don't handle more complicated cases like x*y*z or (y+z)*x.
            Expr coeff = ex.coeff(var, deg);
            VarSet coeffVars = coeff.vars();

            if (coeffVars.size() > 1) {
                return false; // too complicated
            }

            if (coeffVars.size() == 1) {
                Var coeffVar = *coeffVars.begin();
                monomials.insert(coeffVar * var);

                // Check if var also occurs alone, e.g. for (1+y)*x, we also have 1*x.
                // So we are interested in the absolute coefficient of the coeff (1+y).
                if (!coeff.coeff(coeffVar, 0).isZero()) {
                    monomials.insert(var);
                }

            } else {
                // var occurs only alone
                monomials.insert(var);
            }
        }
    }
    return true;
}


bool Linearize::collectMonomialsInGuard(ExprSet &monomials) const {
    for (const Rel &rel : guard) {
        assert(rel.isIneq());

        if (!collectMonomials(rel.lhs().expand(), monomials)) {
            return false;
        }

        if (!collectMonomials(rel.rhs().expand(), monomials)) {
            return false;
        }
    }
    return true;
}


bool Linearize::collectMonomialsInUpdates(ExprSet &monomials) const {
    for (const Subs &update : updates) {
        for (const auto &it : update) {
            if (!collectMonomials(it.second.expand(), monomials)) {
                return false;
            }
        }
    }
    return true;
}


bool Linearize::needsLinearization(const ExprSet &monomials) const {
    auto isNonlinear = [](const Expr &term) { return !term.isLinear(); };
    return std::any_of(monomials.begin(), monomials.end(), isNonlinear);
}


bool Linearize::checkForConflicts(const ExprSet &monomials) const {
    VarSet vars;
    for (const Expr &term : monomials) {
        for (Var var : term.vars()) {
            // If we already know this variable, we have a conflict,
            // since we cannot replace a variable in two different ways
            // (or replace a variable which also occurs linearly).
            if (vars.count(var) > 0) {
                return false;
            }

            // If the variable is updated, we cannot replace it
            // (note that we must replace term whenever term is not linear, i.e., not a single variable)
            if (!term.isLinear() && MeteringToolbox::isUpdatedByAny(var, updates)) {
                return false;
            }

            // Avoid replacing temporary variables, as the replacement would be non-temporary.
            // It would probably be sound to do the replacement, but since this case is probably
            // rare, we disallow it for now. We also have backward accel (works without linearization).
            if (!term.isLinear() && varMan.isTempVar(var)) {
                return false;
            }

            // Otherwise the replacement is safe
            vars.insert(var);
        }
    }
    return true;
}


ExprMap Linearize::buildSubstitution(const ExprSet &monomials) {
    ExprMap res;
    for (const Expr &term: monomials) {
        if (term.isVar()) {
            continue;

        } else if (term.isPow()) {
            Var base = term.op(0).toVar();
            assert(term.op(1).isInt());
            GiNaC::numeric exponent = term.op(1).toNum();

            Var fresh = varMan.addFreshVariable(base.get_name() + to_string(exponent.to_int()));
            res.put(term, fresh);

            // Remember that e.g. x^2 is always nonnegative
            if (exponent.is_even()) {
                additionalGuard.push_back(fresh >= 0);
            }

        } else {
            assert(term.arity() == 2 && term.isMul()); // form x*y
            Var x = term.op(0).toVar();
            Var y = term.op(1).toVar();

            Var fresh = varMan.addFreshVariable(x.get_name() + y.get_name());
            res.put(term, fresh);
        }
    }
    return res;
}


void Linearize::applySubstitution(const ExprMap &subs) {
    GuardList newGuard;
    for (const Rel &rel : guard) {
        newGuard.push_back(rel.expand().replace(subs));
    }
    guard = newGuard;

    std::vector<Subs> newUpdates;
    for (const Subs &update : updates) {
        Subs up;
        for (const auto &it : update) {
            up.put(it.first, it.second.expand().replace(subs));
        }
        newUpdates.push_back(up);
    }
    updates = newUpdates;
}


ExprMap Linearize::reverseSubstitution(const ExprMap &subs) {
    ExprMap reverseSubs;
    for (auto it : subs) {
        assert(it.second.isVar());
        reverseSubs.put(it.second, it.first);
    }
    return reverseSubs;
}


option<ExprMap> Linearize::linearizeGuardUpdates(VarMan &varMan, GuardList &guard, std::vector<Subs> &updates) {

    Linearize lin(guard, updates, varMan);

    // Collect all nonlinear terms that have to be replaced (if possible)
    ExprSet monomials;
    if (!lin.collectMonomialsInGuard(monomials)) {
        return {};
    }
    if (!lin.collectMonomialsInUpdates(monomials)) {
        return {};
    }

    // If everything is linear, there is nothing to do
    if (!lin.needsLinearization(monomials)) {
        return ExprMap(); // empty substitution
    }

    // Check if we are allowed to perform substitutions
    if (!Config::ForwardAccel::AllowLinearization) {
        return {};
    }

    // Check if it is safe to replace all nonlinear terms
    if (!lin.checkForConflicts(monomials)) {
        return {};
    }

    // Construct the replacement and apply it
    ExprMap subs = lin.buildSubstitution(monomials);
    lin.applySubstitution(subs);

    // Add the additional guard (to retain the information that e.g. x^2 is nonnegative)
    for (Rel rel : lin.additionalGuard) {
        guard.push_back(rel);
    }

    return lin.reverseSubstitution(subs);
}
