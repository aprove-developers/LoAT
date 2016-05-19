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

#include"guardtoolbox.h"

#include "debug.h"
#include "its.h"
#include "itrs/itrs.h"

using namespace std;


bool GuardToolbox::isValidGuard(const GuardList &guard) {
    for (const Expression &ex : guard) {
        if (!GiNaC::is_a<GiNaC::relational>(ex) || ex.nops() != 2) return false;
        if (ex.info(GiNaC::info_flags::relation_not_equal)) return false;
    }
    return true;
}


bool GuardToolbox::isPolynomialGuard(const GuardList &guard, const GiNaC::lst &vars) {
    for (const Expression &ex : guard) {
        if (!ex.lhs().is_polynomial(vars) || !ex.rhs().is_polynomial(vars)) return false;
    }
    return true;
}


bool GuardToolbox::isEquality(const Expression &term) {
    assert(GiNaC::is_a<GiNaC::relational>(term));
    return term.info(GiNaC::info_flags::relation_equal);
}


bool GuardToolbox::isEquality(const TT::Expression &term) {
    assert(term.info(TT::InfoFlag::Relation));
    return term.info(TT::InfoFlag::RelationEqual);
}


bool GuardToolbox::isValidInequality(const Expression &term) {
    if (!GiNaC::is_a<GiNaC::relational>(term) || term.nops() != 2) return false;
    if (term.info(GiNaC::info_flags::relation_equal)) return false;
    if (term.info(GiNaC::info_flags::relation_not_equal)) return false;
    return true;
}


bool GuardToolbox::isValidInequality(const TT::Expression &term) {
    if (!term.info(TT::InfoFlag::Relation) || term.nops() != 2) return false;
    if (term.info(TT::InfoFlag::RelationEqual)) return false;
    if (term.info(TT::InfoFlag::RelationNotEqual)) return false;
    return true;
}


bool GuardToolbox::isNormalizedInequality(const Expression &term) {
    return isValidInequality(term)
           && term.info(GiNaC::info_flags::relation_greater)
           && term.rhs().is_zero();
}


Expression GuardToolbox::replaceLhsRhs(const Expression &term, Expression lhs, Expression rhs) {
    assert(isValidInequality(term));
    if (term.info(GiNaC::info_flags::relation_less)) return lhs < rhs;
    if (term.info(GiNaC::info_flags::relation_less_or_equal)) return lhs <= rhs;
    if (term.info(GiNaC::info_flags::relation_greater)) return lhs > rhs;
    if (term.info(GiNaC::info_flags::relation_greater_or_equal)) return lhs >= rhs;
    unreachable();
}


bool GuardToolbox::isLinearInequality(const Expression &term, const GiNaC::lst &vars) {
    if (!isValidInequality(term)) return false;
    return Expression(term.lhs()).isLinear(vars) && Expression(term.rhs()).isLinear(vars);
}


bool GuardToolbox::containsFreeVar(const ITSProblem &its, const Expression &term) {
    for (const string &name : term.getVariableNames()) {
        if (its.isFreeVar(its.getVarindex(name))) return true;
    }
    return false;
}


bool GuardToolbox::containsFreeVar(const ITRSProblem &itrs, const Expression &term) {
    for (const string &name : term.getVariableNames()) {
        if (itrs.isFreeVar(itrs.getVarindex(name))) return true;
    }
    return false;
}


Expression GuardToolbox::makeLessEqual(Expression term) {
    assert(isValidInequality(term));

    //flip > or >=
    if (term.info(GiNaC::info_flags::relation_greater)) {
        term = term.rhs() < term.lhs();
    } else if (term.info(GiNaC::info_flags::relation_greater_or_equal)) {
        term = term.rhs() <= term.lhs();
    }

    //change < to <=, assuming integer arithmetic
    if (term.info(GiNaC::info_flags::relation_less)) {
        term = term.lhs() <= (term.rhs() - 1);
    }

    assert(term.info(GiNaC::info_flags::relation_less_or_equal));
    return term;
}


TT::Expression GuardToolbox::makeLessEqual(TT::Expression term) {
    assert(isValidInequality(term));

    //flip > or >=
    if (term.info(TT::InfoFlag::RelationGreater)) {
        term = term.op(1) < term.op(0);
    } else if (term.info(TT::InfoFlag::RelationGreaterEqual)) {
        term = term.op(1) <= term.op(0);
    }

    //change < to <=, assuming integer arithmetic
    if (term.info(TT::InfoFlag::RelationLess)) {
        term = term.op(0) <= (term.op(1) - 1);
    }

    assert(term.info(TT::InfoFlag::RelationLessEqual));
    return term;
}


Expression GuardToolbox::makeGreater(Expression term) {
    assert(isValidInequality(term));

    //flip < or <=
    if (term.info(GiNaC::info_flags::relation_less)) {
        term = term.rhs() > term.lhs();
    } else if (term.info(GiNaC::info_flags::relation_less_or_equal)) {
        term = term.rhs() >= term.lhs();
    }

    //change >= to >, assuming integer arithmetic
    if (term.info(GiNaC::info_flags::relation_greater_or_equal)) {
        term = (term.lhs() + 1) > term.rhs();
    }

    assert(term.info(GiNaC::info_flags::relation_greater));
    return term;
}


Expression GuardToolbox::normalize(Expression term) {
    assert(isValidInequality(term));

    Expression greater = makeGreater(term);
    Expression normalized = (greater.lhs() - greater.rhs()) > 0;

    assert(isNormalizedInequality(normalized));
    return normalized;
}


Expression GuardToolbox::turnToLess(Expression term) {
    assert(term.info(GiNaC::info_flags::relation_equal)
           || isValidInequality(term));

    if (term.info(GiNaC::info_flags::relation_greater_or_equal)) {
        term = term.rhs() <= term.lhs();
    } else if (term.info(GiNaC::info_flags::relation_greater)) {
        term = term.rhs() < term.lhs();
    }

    return term;
}


Expression GuardToolbox::splitVariablesAndConstants(const Expression &term) {
    assert(isValidInequality(term));
    assert(term.info(GiNaC::info_flags::relation_less_or_equal));

    //move everything to lhs
    GiNaC::ex newLhs = term.lhs() - term.rhs();
    GiNaC::ex newRhs = 0;

    //move all numerical constants back to rhs
    newLhs = newLhs.expand();
    if (GiNaC::is_a<GiNaC::add>(newLhs)) {
        for (int i=0; i < newLhs.nops(); ++i) {
            if (GiNaC::is_a<GiNaC::numeric>(newLhs.op(i))) {
                newRhs = newRhs - newLhs.op(i);
            }
        }
    }
    newLhs = newLhs + newRhs;
    return newLhs <= newRhs;

}


Expression GuardToolbox::negateLessEqualInequality(const Expression &term) {
    assert(isValidInequality(term));
    assert(term.info(GiNaC::info_flags::relation_less_or_equal));
    return (-term.lhs()) <= (-term.rhs())-1;
}


bool GuardToolbox::isTrivialInequality(const Expression &term) {
    assert(term.info(GiNaC::info_flags::relation_less_or_equal));
    using namespace GiNaC;

    if (is_a<numeric>(term.lhs()) && is_a<numeric>(term.rhs())) {
        numeric lhsNum = ex_to<numeric>(term.lhs());
        numeric rhsNum = ex_to<numeric>(term.rhs());
        if (lhsNum.is_equal(rhsNum)) return true;
        if (lhsNum.is_integer() && rhsNum.is_integer() && lhsNum.to_int() <= rhsNum.to_int()) return true;
    } else {
        if ((term.lhs()-term.rhs()).is_zero()) return true;
    }

    return false;
}


bool GuardToolbox::isTrivialInequality(const TT::Expression &term) {
    assert(term.info(TT::InfoFlag::RelationLessEqual));
    using namespace GiNaC;

    TT::Expression lhs = term.op(0);
    TT::Expression rhs = term.op(1);
    if (lhs.info(TT::InfoFlag::Number) && rhs.info(TT::InfoFlag::Number)) {
        numeric lhsNum = ex_to<numeric>(lhs.toGiNaC());
        numeric rhsNum = ex_to<numeric>(rhs.toGiNaC());
        if (lhsNum.is_equal(rhsNum)) return true;
        if (lhsNum.is_integer() && rhsNum.is_integer() && lhsNum.to_int() <= rhsNum.to_int()) return true;
    } else {
        // toGiNaC(true) substitutes function calls by (different) variables
        if ((lhs.toGiNaC(true) - rhs.toGiNaC(true)).is_zero()) return true;
    }

    return false;
}


bool GuardToolbox::solveTermFor(Expression &term, const ExprSymbol &var, PropagationLevel level) {
    assert(!GiNaC::is_a<GiNaC::relational>(term));

    if (term.degree(var) != 1) return false;

    Expression c = term.coeff(var);
    if (level != Nonlinear) {
        if (!GiNaC::is_a<GiNaC::numeric>(c)) return false;
        if (level == NoCoefficients) {
            if (c.compare(1) != 0 && c.compare(-1) != 0) return false;
        }
    }

    term = (term - c*var) / (-c);
    return true;
}


bool GuardToolbox::propagateEqualities(const ITSProblem &its, GuardList &guard, PropagationLevel maxlevel, PropagationFreevar freevar,
                                       GiNaC::exmap *subs, function<bool(const ExprSymbol &)> allowFunc) {
    GiNaC::exmap varSubs;
    for (int i=0; i < guard.size(); ++i) {
        Expression ex = guard[i].subs(varSubs);
        if (!GiNaC::is_a<GiNaC::relational>(ex) || !ex.info(GiNaC::info_flags::relation_equal)) continue;

        Expression target = ex.rhs() - ex.lhs();
        if (!target.is_polynomial(its.getGinacVarList())) continue;

        //check if equation can be solved for any single variable
        for (int level=NoCoefficients; level <= (int)maxlevel; ++level) {
            for (const ExprSymbol &var : target.getVariables()) {
                if (!allowFunc(var)) continue;

                //solve target for var (result is in target)
                if (!solveTermFor(target,var,(PropagationLevel)level)) continue;

                //disallow replacing non-free vars by a term containing free vars
                if (freevar == NoFreeOnRhs) {
                    if (!its.isFreeVar(its.getVarindex(var.get_name())) && containsFreeVar(its,target)) continue;
                }

                //remove current equality (ok while iterating by index)
                guard.erase(guard.begin() + i);
                i--;

                varSubs[var] = target;
                goto next;
            }
        }
        next:;
    }
    //apply substitution to guard and update
    for (Expression &ex: guard) {
        ex = ex.subs(varSubs);
    }
    bool res = !varSubs.empty();
    if (subs) *subs = std::move(varSubs);
    return res;
}


bool GuardToolbox::propagateEqualities(const ITRSProblem &itrs, TT::ExpressionVector &guard, PropagationLevel maxlevel, PropagationFreevar freevar,
                                       GiNaC::exmap *subs, function<bool(const ExprSymbol &)> allowFunc) {
    GiNaC::exmap varSubs;
    for (int i=0; i < guard.size(); ++i) {
        // the guard must not contain any function symbols
        Expression ex = guard[i].toGiNaC().subs(varSubs);
        if (!GiNaC::is_a<GiNaC::relational>(ex) || !ex.info(GiNaC::info_flags::relation_equal)) continue;

        Expression target = ex.rhs() - ex.lhs();
        if (!target.is_polynomial(itrs.getGinacVarList())) continue;

        //check if equation can be solved for any single variable
        for (int level=NoCoefficients; level <= (int)maxlevel; ++level) {
            for (const ExprSymbol &var : target.getVariables()) {
                if (!allowFunc(var)) continue;

                //solve target for var (result is in target)
                if (!solveTermFor(target,var,(PropagationLevel)level)) continue;

                //disallow replacing non-free vars by a term containing free vars
                if (freevar == NoFreeOnRhs) {
                    if (!itrs.isFreeVar(itrs.getVarindex(var.get_name())) && containsFreeVar(itrs,target)) continue;
                }

                //remove current equality (ok while iterating by index)
                guard.erase(guard.begin() + i);
                i--;

                varSubs[var] = target;
                goto next;
            }
        }
        next:;
    }
    //apply substitution to guard and update
    for (TT::Expression &ex: guard) {
        ex = ex.substitute(varSubs);
    }
    bool res = !varSubs.empty();
    if (subs) *subs = std::move(varSubs);
    return res;
}


bool GuardToolbox::eliminateByTransitiveClosure(GuardList &guard, const GiNaC::lst &vars, bool removeHalfBounds,
                                                function<bool(const ExprSymbol &)> allowFunc) {
    //get all variables that appear in an inequality
    ExprSymbolSet tryVars;
    for (const Expression &ex : guard) {
        if (!isValidInequality(ex) || !(ex.lhs()-ex.rhs()).is_polynomial(vars)) continue;
        ex.collectVariables(tryVars);
    }

    //for each variable, try if we can eliminate every occurrence. Otherwise do nothing.
    bool changed = false;
    for (const ExprSymbol &var : tryVars) {
        if (!allowFunc(var)) continue;

        vector<Expression> varLessThan, varGreaterThan; //var <= expr and var >= expr
        vector<int> guardTerms; //indices of guard terms that can be removed if succesfull

        for (int i=0; i < guard.size(); ++i) {
            //check if this guard must be used for var
            const Expression &ex = guard[i];
            if (!ex.has(var)) continue;
            if (!isValidInequality(ex) || !(ex.lhs()-ex.rhs()).is_polynomial(vars)) goto abort;

            //make less equal
            Expression target = makeLessEqual(ex);
            target = target.lhs() - target.rhs();
            if (!target.has(var)) continue; //might have changed, e.h. x <= x

            //check coefficient and direction
            Expression c = target.coeff(var);
            if (c.compare(1) != 0 && c.compare(-1) != 0) goto abort;
            if (c.compare(1) == 0) {
                varLessThan.push_back( -(target-var) );
            } else {
                varGreaterThan.push_back( target+var );
            }
            guardTerms.push_back(i);
        }
        if (guardTerms.empty()) goto abort;
        if (!removeHalfBounds && (varLessThan.empty() || varGreaterThan.empty())) goto abort;

        //success: remove lower <= x and x <= upper as they will be replaced
        while (!guardTerms.empty()) {
            guard.erase(guard.begin() + guardTerms.back());
            guardTerms.pop_back();
        }
        //add new transitive guard terms
        for (const Expression &upper : varLessThan) {
            for (const Expression &lower : varGreaterThan) {
                //lower <= var <= upper --> lower <= upper
                guard.push_back(lower <= upper);
            }
        }
        changed = true;

abort:  ; //this symbol could not be eliminated, try the next one
    }
    return changed;
}


bool GuardToolbox::eliminateByTransitiveClosure(const ITRSProblem &itrs, TT::ExpressionVector &guard, const GiNaC::lst &vars, bool removeHalfBounds,
                                                function<bool(const ExprSymbol &)> allowFunc) {
    //get all variables that appear in an inequality
    ExprSymbolSet tryVars;
    for (const TT::Expression &ex : guard) {
        Expression asGiNaC = ex.toGiNaC();
        if (!isValidInequality(asGiNaC) || !(asGiNaC.lhs()-asGiNaC.rhs()).is_polynomial(vars)) continue;
        asGiNaC.collectVariables(tryVars);
    }

    //for each variable, try if we can eliminate every occurrence. Otherwise do nothing.
    bool changed = false;
    for (const ExprSymbol &var : tryVars) {
        if (!allowFunc(var)) continue;

        vector<Expression> varLessThan, varGreaterThan; //var <= expr and var >= expr
        vector<int> guardTerms; //indices of guard terms that can be removed if succesfull

        for (int i=0; i < guard.size(); ++i) {
            //check if this guard must be used for var
            const Expression ex = guard[i].toGiNaC();
            if (!ex.has(var)) continue;
            if (!isValidInequality(ex) || !(ex.lhs()-ex.rhs()).is_polynomial(vars)) goto abort;

            //make less equal
            Expression target = makeLessEqual(ex);
            target = target.lhs() - target.rhs();
            if (!target.has(var)) continue; //might have changed, e.h. x <= x

            //check coefficient and direction
            Expression c = target.coeff(var);
            if (c.compare(1) != 0 && c.compare(-1) != 0) goto abort;
            if (c.compare(1) == 0) {
                varLessThan.push_back( -(target-var) );
            } else {
                varGreaterThan.push_back( target+var );
            }
            guardTerms.push_back(i);
        }
        if (guardTerms.empty()) goto abort;
        if (!removeHalfBounds && (varLessThan.empty() || varGreaterThan.empty())) goto abort;

        //success: remove lower <= x and x <= upper as they will be replaced
        while (!guardTerms.empty()) {
            guard.erase(guard.begin() + guardTerms.back());
            guardTerms.pop_back();
        }
        //add new transitive guard terms
        for (const Expression &upper : varLessThan) {
            for (const Expression &lower : varGreaterThan) {
                //lower <= var <= upper --> lower <= upper
                guard.push_back(TT::Expression(itrs, lower <= upper));
            }
        }
        changed = true;

abort:  ; //this symbol could not be eliminated, try the next one
    }
    return changed;
}


bool GuardToolbox::findEqualities(GuardList &guard) {
    vector<pair<int,Expression>> terms; //inequalities from the guard, with the associated index in guard
    map<int,pair<int,Expression>> matches; //maps index in guard to a second index in guard, which can be replaced by Expression

    for (int i=0; i < guard.size(); ++i) {
        if (isEquality(guard[i])) continue;
        Expression term = makeLessEqual(guard[i]);
        term = term.lhs() - term.rhs();
        for (const auto &prev : terms) {
            if ((prev.second + term).is_zero()) {
                matches[prev.first] = make_pair(i,prev.second);
            }
        }
        terms.push_back(make_pair(i,term));
    }

    if (matches.empty()) return false;

    GuardList res;
    set<int> ignore;
    for (int i=0; i < guard.size(); ++i) {
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


bool GuardToolbox::findEqualities(TT::ExpressionVector &guard) {
    vector<pair<int,TT::Expression>> terms; //inequalities from the guard, with the associated index in guard
    map<int,pair<int,TT::Expression>> matches; //maps index in guard to a second index in guard, which can be replaced by Expression

    for (int i=0; i < guard.size(); ++i) {
        if (isEquality(guard[i])) continue;
        TT::Expression term = makeLessEqual(guard[i]);
        term = term.op(0) - term.op(1);
        for (const auto &prev : terms) {
            if ((prev.second + term).toGiNaC(true).is_zero()) {
                matches[prev.first] = make_pair(i,prev.second);
            }
        }
        terms.push_back(make_pair(i,term));
    }

    if (matches.empty()) return false;

    TT::ExpressionVector res;
    set<int> ignore;
    for (int i=0; i < guard.size(); ++i) {
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


GiNaC::exmap GuardToolbox::composeSubs(const GiNaC::exmap &f, const GiNaC::exmap &g) {
    GiNaC::exmap substitution;

    for (auto const &pair : g) {
        substitution.insert(std::make_pair(pair.first, pair.second.subs(f)));
    }

    for (auto const &pair : f) {
        if (substitution.count(pair.first) == 0) {
            substitution.insert(pair);
        }
    }

    return substitution;
}