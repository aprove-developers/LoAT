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

#include "guardtoolbox.hpp"
#include "../its/rule.hpp"

using namespace std;


bool GuardToolbox::isTrivialImplication(const Rel &a, const Rel &b) {

    // an equality can only be implied by an equality
    if (b.isEq()) {
        if (!a.isEq()) return false;

        Expr aDiff = a.rhs() - a.lhs();
        Expr bDiff = b.rhs() - b.lhs();
        return (aDiff - bDiff).expand().isZero();
    } else if (a.isIneq()) {
        if (a.isStrict() == b.isStrict()) {
            if ((a.toG().makeRhsZero().lhs() <= b.toG().makeRhsZero().lhs()).isTriviallyTrue()) {
                return true;
            }
        } else if (!a.isStrict() && a.isPoly()) {
            if ((a.toGt().makeRhsZero().lhs() <= b.toG().makeRhsZero().lhs()).isTriviallyTrue()) {
                return true;
            }
        } else if (b.isPoly()) {
            if ((a.toG().makeRhsZero().lhs() <= b.toGt().makeRhsZero().lhs()).isTriviallyTrue()) {
                return true;
            }
        }
    } else if (a.isEq()) {
        Expr aDiff = a.rhs() - a.lhs();
        Expr bLhs = b.toG().lhs() - b.toG().rhs();
        if (b.isStrict()) {
            return (aDiff < bLhs).isTriviallyTrue() || ((-aDiff) < bLhs).isTriviallyTrue();
        } else {
            return (aDiff <= bLhs).isTriviallyTrue() || ((-aDiff) <= bLhs).isTriviallyTrue();
        }
    }
    return false;
}


option<Expr> GuardToolbox::solveTermFor(Expr term, const Var &var, SolvingLevel level) {
    // expand is needed before using degree/coeff
    term = term.expand();

    // we can only solve linear expressions...
    if (term.degree(var) != 1) return {};

    // ...with rational coefficients
    Expr c = term.coeff(var);
    if (!c.isRationalConstant()) return {};

    bool trivialCoeff = (c.compare(1) == 0 || c.compare(-1) == 0);

    if (level == TrivialCoeffs && !trivialCoeff) {
        return {};
    }

    term = (term - c*var) / (-c);

    // If c is trivial, we don't have to check if the result maps to int,
    // since we assume that all constraints in the guard map to int.
    // So if c is trivial, we can also handle non-polynomial terms.
    if (level == ResultMapsToInt && !trivialCoeff) {
        if (!term.isPoly() || !term.isIntegral()) return {};
    }

    // we assume that terms in the guard map to int, make sure this is the case
    if (trivialCoeff) {
        assert(!term.isPoly() || term.isIntegral());
    }

    return term;
}


bool GuardToolbox::propagateEqualities(const VarMan &varMan, Rule &rule, SolvingLevel maxlevel, SymbolAcceptor allow) {
    ExprMap varSubs;
    GuardList &guard = rule.getGuardMut();

    for (unsigned int i=0; i < guard.size(); ++i) {
        Rel rel = guard[i].subs(varSubs);
        if (!rel.isEq()) continue;

        Expr target = rel.rhs() - rel.lhs();
        if (!target.isPoly()) continue;

        // Check if equation can be solved for any single variable.
        // We prefer to solve for variables where this is easy,
        // e.g. in x+2*y = 0 we prefer x since x has only the trivial coefficient 1.
        for (int level=TrivialCoeffs; level <= (int)maxlevel; ++level) {
            for (const Var &var : target.vars()) {
                if (!allow(var)) continue;

                //solve target for var (result is in target)
                auto optSolved = solveTermFor(target,var,(SolvingLevel)level);
                if (!optSolved) continue;
                Expr solved = optSolved.get();

                //disallow replacing non-free vars by a term containing free vars
                //could be unsound, as free vars can lead to unbounded complexity
                if (!varMan.isTempVar(var) && containsTempVar(varMan, solved)) continue;

                //remove current equality (ok while iterating by index)
                guard.erase(guard.begin() + i);
                i--;

                //extend the substitution, use compose in case var occurs on some rhs of varSubs
                varSubs.put(var, solved);
                varSubs = varSubs.compose(varSubs);
                goto next;
            }
        }
        next:;
    }

    //apply substitution to the entire rule
    rule.applySubstitution(varSubs);
    return !varSubs.empty();
}


bool GuardToolbox::eliminateByTransitiveClosure(GuardList &guard, bool removeHalfBounds, SymbolAcceptor allow) {
    //get all variables that appear in an inequality
    VarSet tryVars;
    for (const Rel &rel : guard) {
        if (!rel.isIneq() || !rel.isPoly()) continue;
        rel.collectVariables(tryVars);
    }

    //for each variable, try if we can eliminate every occurrence. Otherwise do nothing.
    bool changed = false;
    for (const Var &var : tryVars) {
        if (!allow(var)) continue;

        vector<Expr> varLessThan, varGreaterThan; //var <= expr and var >= expr
        vector<int> guardTerms; //indices of guard terms that can be removed if successful

        for (unsigned int i=0; i < guard.size(); ++i) {
            //check if this guard must be used for var
            const Rel &rel = guard[i];
            if (!rel.has(var)) continue;
            if (!rel.isIneq() || !rel.isPoly()) goto abort; // contains var, but cannot be handled

            Expr target = rel.toLeq().makeRhsZero().lhs();
            if (!target.has(var)) continue; // might have changed, e.h. x <= x

            //check coefficient and direction
            Expr c = target.expand().coeff(var);
            if (c.compare(1) != 0 && c.compare(-1) != 0) goto abort;
            if (c.compare(1) == 0) {
                varLessThan.push_back( -(target-var) );
            } else {
                varGreaterThan.push_back( target+var );
            }
            guardTerms.push_back(i);
        }

        // abort if no eliminations can be performed
        if (guardTerms.empty()) goto abort;
        if (!removeHalfBounds && (varLessThan.empty() || varGreaterThan.empty())) goto abort;

        //success: remove lower <= x and x <= upper as they will be replaced
        while (!guardTerms.empty()) {
            guard.erase(guard.begin() + guardTerms.back());
            guardTerms.pop_back();
        }

        //add new transitive guard terms lower <= upper
        for (const Expr &upper : varLessThan) {
            for (const Expr &lower : varGreaterThan) {
                guard.push_back(lower <= upper);
            }
        }
        changed = true;

abort:  ; //this symbol could not be eliminated, try the next one
    }
    return changed;
}


bool GuardToolbox::makeEqualities(GuardList &guard) {
    vector<pair<int,Expr>> terms; //inequalities from the guard, with the associated index in guard
    map<int,pair<int,Expr>> matches; //maps index in guard to a second index in guard, which can be replaced by Expression

    // Find matching constraints "t1 <= 0" and "t2 <= 0" such that t1+t2 is zero
    for (unsigned int i=0; i < guard.size(); ++i) {
        if (guard[i].isEq()) continue;
        if (!guard[i].isPoly() && guard[i].isStrict()) continue;
        Expr term = guard[i].toLeq().makeRhsZero().lhs();
        for (const auto &prev : terms) {
            if ((prev.second + term).isZero()) {
                matches.emplace(prev.first, make_pair(i,prev.second));
            }
        }
        terms.push_back(make_pair(i,term));
    }

    if (matches.empty()) return false;

    // Construct the new guard by keeping unmatched constraint
    // and replacing matched pairs by an equality constraint.
    // This code below mostly retains the order of the constraints.
    GuardList res;
    set<int> ignore;
    for (unsigned int i=0; i < guard.size(); ++i) {
        //ignore multiple equalities as well as the original second inequality
        if (ignore.count(i) > 0) continue;

        auto it = matches.find(i);
        if (it != matches.end()) {
            res.push_back(Rel::buildEq(it->second.second, 0));
            ignore.insert(it->second.first);
        } else {
            res.push_back(guard[i]);
        }
    }
    res.swap(guard);
    return true;
}
