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

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <ginac/ginac.h>
#include <string>
#include <vector>
#include <purrs.hh>

#include "../util/exceptions.hpp"
#include "complexity.hpp"
#include "../util/option.hpp"

namespace Purrs = Parma_Recurrence_Relation_Solver;

class Expr;
class Recurrence;
class ExprMap;
class Rel;

struct Expr_is_less {
    bool operator() (const Expr &lh, const Expr &rh) const;
};

using Var = GiNaC::symbol;
using VarSet = std::set<Var, GiNaC::ex_is_less>;
using ExprSet = std::set<Expr, Expr_is_less>;
using RelSet = std::set<Rel>;
template <typename T>
using VarMap = std::map<Var, T, GiNaC::ex_is_less>;

std::ostream& operator<<(std::ostream &s, const ExprMap &map);

/**
 * Class for arithmetic expressions.
 */
class Expr {

    /*
     * We use PURRS to solve recurrence relatios, which also uses GiNaC.
     * Declaring it as a friend allows us to direclty work on the encapsulated GiNaC::ex when constructing recurrence relations.
     */
    friend class Recurrence;

    /*
     * An ExprMap encapsulates a GiNaC::exmap, which can directly be applied to the encapsulated GiNaC::ex.
     */
    friend class ExprMap;

public:

    /**
     * possible types of variables
     */
    enum Type {Int, Rational};

    /**
     * Special variable that represents the cost of non-terminating computations.
     */
    static const Var NontermSymbol;

    /**
     * @return A wildcard for constructing patterns.
     */
    static Expr wildcard(uint label);

    Expr() : Expr(GiNaC::ex()) {}
    Expr(const GiNaC::basic &other) : Expr(GiNaC::ex(other)) {}
    Expr(const GiNaC::ex &ex) : ex(ex) {}
    Expr(long i): ex(i) {}

    /**
     * @brief Applies a substitution via side-effects.
     * @deprecated use subs instead
     */
    void applySubs(const ExprMap &subs);

    /**
     * @brief Computes all matches of the given pattern.
     * @return True iff there was at least one match.
     */
    bool findAll(const Expr &pattern, ExprSet &found) const;

    /**
     * @return True iff this expression is equal (resp. evaluates) to the given variable
     */
    bool equals(const Var &var) const;

    /**
     * @return True iff this expression is equal to NontermSymbol.
     */
    bool isNontermSymbol() const;

    /**
     * @return True iff this expression is a linear polynomial wrt. the given variables (resp. all variables, if vars is empty).
     */
    bool isLinear(const option<VarSet> &vars = option<VarSet>()) const;

    /**
     * @return True iff this expression is a polynomial.
     */
    bool isPoly() const;

    /**
     * @return True iff this expression is polynomial where all coefficients are integers.
     */
    bool isIntPoly() const;

    /**
     * @return True iff this expression is an integer value (and thus a constant).
     */
    bool isInt() const;

    /**
     * @return True iff this expression is a rational number (and thus a constant).
     */
    bool isRationalConstant() const;

    /**
     * @return True iff this expression is a rational number, but no integer constant.
     */
    bool isNonIntConstant() const;

    /**
     * @return True iff this expression is a power where the exponent is a natural number > 1.
     */
    bool isNaturalPow() const;

    /**
     * @return The highest degree of any variable in this expression.
     * @note For polynomials only.
     */
    int maxDegree() const;

    /**
     * @brief Collects all variables that occur in this expression.
     */
    void collectVars(VarSet &res) const;

    /**
     * @return The set of all variables that occur in this expression.
     */
    VarSet vars() const;

    /**
     * @return True iff this expression contains a variable that satisfies the given predicate.
     * @param A function of type `const Var & => bool`.
     */
    template <typename P>
    bool hasVarWith(P predicate) const {
        struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
            SymbolVisitor(P predicate) : predicate(predicate) {}
            void visit(const Var &sym) {
                if (!res && predicate(sym)) {
                    res = true;
                }
            }
            bool result() const {
                return res;
            }
        private:
            bool res = false;
            P predicate;
        };

        SymbolVisitor visitor(predicate);
        traverse(visitor);
        return visitor.result();
    }

    /**
     * @return True iff this expression does not contain any variables.
     */
    bool isGround() const;

    /**
     * @return True iff this expression contains exactly one variable.
     */
    bool isUnivariate() const;

    /**
     * @return Some variable that occurs in this Expression.
     * @note Only for non-ground expressions.
     */
    Var someVar() const;

    /**
     * @return True iff this expression is ground or univariate.
     */
    bool isNotMultivariate() const;

    /**
     * @return True iff this expression contains at least two variable.
     */
    bool isMultivariate() const;

    /**
     * @return An estimate of the asymptotic complexity of this expression.
     * @note This should be an over-approximation, but there are no guarantees.
     * @note May return Complexity::CpxUnknown.
     */
    Complexity toComplexity() const;

    /**
     * @return A string representation of this expression.
     */
    std::string toString() const;

    bool equals(const Expr &that) const;
    int degree(const Var &var) const;

    /**
     * @return The minimal degree of all monomials wrt. var.
     */
    int ldegree(const Var &var) const;

    Expr coeff(const Var &var, int degree = 1) const;

    /**
     * @return The coefficient of the monomial whose degree wrt. var is ldegree(var).
     */
    Expr lcoeff(const Var &var) const;

    /**
     * @return A normalized version of this expression up to the order of monomials.
     * @note No guarantees for non-polynomial expressions.
     */
    Expr expand() const;

    /**
     * @return True iff some subexpression matches the given pattern.
     */
    bool has(const Expr &pattern) const;

    /**
     * @return True iff this is 0.
     */
    bool isZero() const;

    /**
     * @return True iff this is a variable.
     */
    bool isVar() const;

    /**
     * @return True iff this is of the form x^y for some expressions x, y.
     */
    bool isPow() const;

    /**
     * @return True iff this is of the form x*y for some expressions x, y.
     */
    bool isMul() const;

    /**
     * @return True iff this is of the form x+y for some expressions x, y.
     */
    bool isAdd() const;

    /**
     * @return This as a variable.
     * @note For variables only.
     */
    Var toVar() const;

    /**
     * @return This as a number.
     * @note For constants only.
     */
    GiNaC::numeric toNum() const;

    /**
     * @return The i-th operand.
     * @note For function applications whose root symbol has at least arity i+1 only.
     */
    Expr op(uint i) const;

    /**
     * @return The arity of the root symbol.
     * @note For function applications only.
     */
    size_t arity() const;

    /**
     * @return The result of applying the given substitution to this expression.
     * @note The second argument is deprecated.
     */
    Expr subs(const ExprMap &map, uint options = 0) const;

    /**
     * @brief Provides a total order for expressions.
     */
    int compare(const Expr &that) const;

    /**
     * @return The numerator.
     * @note For fractions only.
     */
    Expr numerator() const;

    /**
     * @return The denominator.
     * @note For fractions only.
     */
    Expr denominator() const;

    /**
     * @return True iff this is a polynomial wrt. the given variable.
     */
    bool isPoly(const Var &n) const;

    /**
     * @brief exponentiation
     */
    friend Expr operator^(const Expr &x, const Expr &y);
    friend Expr operator-(const Expr &x);
    friend Expr operator-(const Expr &x, const Expr &y);
    friend Expr operator+(const Expr &x, const Expr &y);
    friend Expr operator*(const Expr &x, const Expr &y);
    friend Expr operator/(const Expr &x, const Expr &y);
    friend Rel operator<(const Expr &x, const Expr &y);
    friend Rel operator>(const Expr &x, const Expr &y);
    friend Rel operator<=(const Expr &x, const Expr &y);
    friend Rel operator>=(const Expr &x, const Expr &y);
    friend Rel operator!=(const Expr &x, const Expr &y);
    friend Rel operator==(const Expr &x, const Expr &y);
    friend std::ostream& operator<<(std::ostream &s, const Expr &e);

private:

    GiNaC::ex ex;

    static Complexity toComplexity(const Expr &term);

    bool match(const Expr &pattern) const;
    void traverse(GiNaC::visitor & v) const;

};

class Rel {
public:

    EXCEPTION(InvalidRelationalExpression,CustomException);

    enum Operator {lt, leq, gt, geq, eq, neq};

    Rel(const Expr &lhs, Operator op, const Expr &rhs);

    Expr lhs() const;
    Expr rhs() const;
    Rel expand() const;
    bool isPoly() const;
    bool isLinear(const option<VarSet> &vars = option<VarSet>()) const;
    bool isInequality() const;
    bool isGreaterThanZero() const;
    Rel toLessEq() const;
    Rel toGreater() const;
    Rel toLessOrLessEq() const;
    Rel splitVariablesAndConstants(const VarSet &params = VarSet()) const;
    bool isTriviallyTrue() const;
    bool isTriviallyFalse() const;
    void collectVariables(VarSet &res) const;
    bool has(const Expr &pattern) const;
    Rel subs(const ExprMap &map, uint options = 0) const;
    void applySubs(const ExprMap &subs);
    std::string toString() const;
    Operator getOp() const;
    VarSet getVariables() const;
    int compare(const Rel &that) const;

    static Rel fromString(const std::string &s, const GiNaC::lst &variables);

    template <typename P>
    bool hasVarWith(P predicate) const {
        return l.hasVarWith(predicate) || r.hasVarWith(predicate);
    }

    /**
     * Given an inequality, transforms it into one of the form lhs > 0
     * @note assumes integer arithmetic to translate e.g. >= to >
     */
    Rel normalizeInequality() const;

    friend Rel operator!(const Rel &x);
    friend bool operator==(const Rel &x, const Rel &y);
    friend bool operator!=(const Rel &x, const Rel &y);
    friend bool operator<(const Rel &x, const Rel &y);
    friend std::ostream& operator<<(std::ostream &s, const Rel &e);

private:

    /**
     * Given any relation, checks if the relation is trivially true or false,
     * by checking if rhs-lhs is a numeric value. If unsure, returns none.
     * @return true/false if the relation is trivially valid/invalid
     */
    option<bool> checkTrivial() const;

    Expr l;
    Expr r;
    Operator op;

};

Rel operator<(const Var &x, const Expr &y);
Rel operator<(const Expr &x, const Var &y);
Rel operator<(const Var &x, const Var &y);
Rel operator>(const Var &x, const Expr &y);
Rel operator>(const Expr &x, const Var &y);
Rel operator>(const Var &x, const Var &y);
Rel operator<=(const Var &x, const Expr &y);
Rel operator<=(const Expr &x, const Var &y);
Rel operator<=(const Var &x, const Var &y);
Rel operator>=(const Var &x, const Expr &y);
Rel operator>=(const Expr &x, const Var &y);
Rel operator>=(const Var &x, const Var &y);

class ExprMap {
    friend class Expr;

public:
    ExprMap();
    ExprMap(const Expr &key, const Expr &val);
    Expr get(const Expr &key) const;
    void put(const Expr &key, const Expr &val);
    std::map<Expr, Expr, Expr_is_less>::const_iterator begin() const;
    std::map<Expr, Expr, Expr_is_less>::const_iterator end() const;
    std::map<Expr, Expr, Expr_is_less>::const_iterator find(const Expr &e) const;
    bool contains(const Expr &e) const;
    bool empty() const;
    ExprMap compose(const ExprMap &that) const;

    friend bool operator<(const ExprMap &x, const ExprMap &y);

private:
    std::map<Expr, Expr, Expr_is_less> map;
    std::map<GiNaC::ex, GiNaC::ex, GiNaC::ex_is_less> ginacMap;

};

#endif // EXPRESSION_H
