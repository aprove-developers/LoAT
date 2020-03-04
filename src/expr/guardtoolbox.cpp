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

bool GuardToolbox::isWellformedGuard(const GuardList &guard) {
    for (const Rel &rel : guard) {
        if (rel.getOp() == Rel::neq) return false;
    }
    return true;
}


bool GuardToolbox::isPolynomialGuard(const GuardList &guard) {
    return std::all_of(guard.begin(), guard.end(), [](const Rel &x){return x.isPolynomial();});
}

bool GuardToolbox::isTrivialImplication(const Rel &a, const Rel &b) {

    // an equality can only be implied by an equality
    if (b.getOp() == Rel::eq) {
        if (a.getOp() != Rel::eq) return false;

        Expression aDiff = a.rhs() - a.lhs();
        Expression bDiff = b.rhs() - b.lhs();
        return (aDiff - bDiff).expand().is_zero();
    }

    Expression bLhs = b.normalizeInequality().lhs(); // b is of the form bLhs > 0
    if (a.getOp() != Rel::eq) {
        Expression aLhs = a.normalizeInequality().lhs(); // a is of the form aLhs > 0
        return (aLhs <= bLhs).isTriviallyTrue(); // then 0 < aLhs <= bLhs, so 0 < bLhs holds
    }

    // if a is an equality, we can use aDiff >= 0 or aDiff <= 0, so we check both
    // note that we need a strict check below, as we want to show bLhs > 0
    Expression aDiff = a.rhs() - a.lhs();
    return (aDiff < bLhs).isTriviallyTrue() || ((-aDiff) < bLhs).isTriviallyTrue();
}


option<Expression> GuardToolbox::solveTermFor(Expression term, const ExprSymbol &var, SolvingLevel level) {
    // expand is needed before using degree/coeff
    term = term.expand();

    // we can only solve linear expressions...
    if (term.degree(var) != 1) return {};

    // ...with rational coefficients
    Expression c = term.coeff(var);
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
        if (!term.isPolynomial() || !mapsToInt(term)) return {};
    }

    // we assume that terms in the guard map to int, make sure this is the case
    if (trivialCoeff) {
        assert(!term.isPolynomial() || mapsToInt(term));
    }

    return term;
}


bool GuardToolbox::propagateEqualities(const VarMan &varMan, Rule &rule, SolvingLevel maxlevel, SymbolAcceptor allow) {
    ExprMap varSubs;
    GuardList &guard = rule.getGuardMut();

    for (unsigned int i=0; i < guard.size(); ++i) {
        Rel rel = guard[i].subs(varSubs);
        if (rel.getOp() != Rel::eq) continue;

        Expression target = rel.rhs() - rel.lhs();
        if (!target.isPolynomial()) continue;

        // Check if equation can be solved for any single variable.
        // We prefer to solve for variables where this is easy,
        // e.g. in x+2*y = 0 we prefer x since x has only the trivial coefficient 1.
        for (int level=TrivialCoeffs; level <= (int)maxlevel; ++level) {
            for (const ExprSymbol &var : target.getVariables()) {
                if (!allow(var)) continue;

                //solve target for var (result is in target)
                auto optSolved = solveTermFor(target,var,(SolvingLevel)level);
                if (!optSolved) continue;
                Expression solved = optSolved.get();

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
    ExprSymbolSet tryVars;
    for (const Rel &rel : guard) {
        if (!rel.isInequality() || !rel.isPolynomial()) continue;
        rel.collectVariables(tryVars);
    }

    //for each variable, try if we can eliminate every occurrence. Otherwise do nothing.
    bool changed = false;
    for (const ExprSymbol &var : tryVars) {
        if (!allow(var)) continue;

        vector<Expression> varLessThan, varGreaterThan; //var <= expr and var >= expr
        vector<int> guardTerms; //indices of guard terms that can be removed if successful

        for (unsigned int i=0; i < guard.size(); ++i) {
            //check if this guard must be used for var
            const Rel &rel = guard[i];
            if (!rel.has(var)) continue;
            if (!rel.isInequality() || !rel.isPolynomial()) goto abort; // contains var, but cannot be handled

            //make less equal
            Rel leq = rel.toLessEq();
            Expression target = leq.lhs() - leq.rhs();
            if (!target.has(var)) continue; // might have changed, e.h. x <= x

            //check coefficient and direction
            Expression c = target.expand().coeff(var);
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
        for (const Expression &upper : varLessThan) {
            for (const Expression &lower : varGreaterThan) {
                guard.push_back(lower <= upper);
            }
        }
        changed = true;

abort:  ; //this symbol could not be eliminated, try the next one
    }
    return changed;
}


bool GuardToolbox::makeEqualities(GuardList &guard) {
    vector<pair<int,Expression>> terms; //inequalities from the guard, with the associated index in guard
    map<int,pair<int,Expression>> matches; //maps index in guard to a second index in guard, which can be replaced by Expression

    // Find matching constraints "t1 <= 0" and "t2 <= 0" such that t1+t2 is zero
    for (unsigned int i=0; i < guard.size(); ++i) {
        if (guard[i].getOp() == Rel::eq) continue;
        Rel leq = guard[i].toLessEq();
        Expression term = leq.lhs() - leq.rhs();
        for (const auto &prev : terms) {
            if ((prev.second + term).is_zero()) {
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
            res.push_back(it->second.second == 0);
            ignore.insert(it->second.first);
        } else {
            res.push_back(guard[i]);
        }
    }
    res.swap(guard);
    return true;
}


bool GuardToolbox::mapsToInt(const Expression &e) {
    assert(e.isPolynomial());

    // shortcut for the common case
    if (e.isPolynomialWithIntegerCoeffs()) {
        return true;
    }

    // collect variables from e into a vector
    vector<ExprSymbol> vars;
    for (ExprSymbol sym : e.getVariables()) {
        vars.push_back(sym);
    }

    // degrees, subs share indices with vars
    vector<int> degrees;
    vector<int> subs;
    Expression expanded = e.expand();
    for (const ExprSymbol &x: vars) {
        degrees.push_back(expanded.degree(x));
        subs.push_back(0);
    }

    while (true) {
        // substitute every variable x_i by the integer subs[i] and check if the result is an integer
        ExprMap currSubs;
        for (unsigned int i = 0; i < degrees.size(); i++) {
            currSubs.put(vars[i], subs[i]);
        }
        Expression res = e.subs(currSubs).expand();
        if (!res.isIntegerConstant()) {
            return false;
        }

        // increase subs (lexicographically) if possible
        // (the idea is that subs takes all possible combinations of 0,...,degree[i]+1 for every entry i)
        bool foundNext = false;
        for (unsigned int i = 0; i < degrees.size(); i++) {
            if (subs[i] >= degrees[i]+1) {
                subs[i] = 0;
            } else {
                subs[i] += 1;
                foundNext = true;
                break;
            }
        }

        if (!foundNext) {
            return true;
        }
    }
}
