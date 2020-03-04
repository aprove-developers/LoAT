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

class Z3Context;
class Expression;
class Recurrence;
class ExprMap;

struct Expression_is_less {
    bool operator() (const Expression &lh, const Expression &rh) const;
};

//Useful typedefs for readability
using ExprSymbol = GiNaC::symbol;
using ExprSymbolSet = std::set<ExprSymbol, GiNaC::ex_is_less>;
using ExpressionSet = std::set<Expression, Expression_is_less>;
template <typename T>
using ExprSymbolMap = std::map<ExprSymbol, T, GiNaC::ex_is_less>;


std::ostream& operator<<(std::ostream &s, const ExprMap &e);

/**
 * Class for arithmetic expressions, can be converted to a z3 expression.
 * Essentially a wrapper around GiNaC::ex which provides additional functionality.
 */
class Expression {
    friend class Recurrence;
public:
    enum Type {Int, Real};

    static const ExprSymbol NontermSymbol; // special symbol "INF" to be used within cost expressions

public:
    /**
     * Creates a new Expression from the given string representation.
     * Unlike GiNaC this can also parse relational expressions (i.e. <,<= etc.).
     * @param variables a list of GiNaC symbols that may occur as variables in s
     */
    static Expression fromString(const std::string &s, const GiNaC::lst &variables);
    EXCEPTION(InvalidRelationalExpression,CustomException);

    /**
     * static helper to cast a ginac expression (which must be a symbol) to a ginac symbol
     */
    static ExprSymbol toSymbol(const GiNaC::ex &x) { return GiNaC::ex_to<GiNaC::symbol>(x); }

public:
    Expression() : Expression(GiNaC::ex()) {}
    Expression(const GiNaC::basic &other) : Expression(GiNaC::ex(other)) {}
    Expression(const GiNaC::ex &ex) : ex(ex) {}
    Expression(int i): ex(i) {}

    /**
     * Applies a substitution. Like GiNaC::subs, but assigns the result to this expression.
     */
    void applySubs(const ExprMap &subs);

    /**
     * Version of ex::find() that searches also in subexpressions of a match
     */
    bool findAll(const Expression &pattern, ExpressionSet &found) const;

    /**
     * Returns true iff this expression is the given variable (up to trivial arithmetic, which is resolved by GiNaC)
     */
    bool equalsVariable(const GiNaC::symbol &var) const;

    /**
     * Checks if this expression is the NONTERM-symbol (used to represent nontermination).
     * This is only a syntactic check.
     */
    bool isNontermSymbol() const;

    /**
     * Returns true iff this expression is linear
     * (e.g. 1/2*x+y is linear, but x^2 or x*y are not considered linear)
     */
    bool isLinear(const option<ExprSymbolSet> &vars = option<ExprSymbolSet>()) const;

    /**
     * Returns true iff this expression is polynomial, e.g. 1/2 * y * x^2 + y^3.
     */
    bool isPolynomial() const;

    /**
     * Returns true iff this expression is polynomial where all coefficients are integers.
     */
    bool isPolynomialWithIntegerCoeffs() const;

    /**
     * Returns true iff this expression is an integer value (and thus a constant).
     */
    bool isIntegerConstant() const;

    /**
     * Returns true iff this expression is a rational number (and thus a constant).
     */
    bool isRationalConstant() const;

    /**
     * Returns true iff this expression is a proper rational number,
     * i.e., a rational number that is not an integer.
     */
    bool isProperRational() const;

    /**
     * Returns true iff this expression is a proper natural power,
     * i.e., of the form expresion^n for some natural n >= 2.
     */
    bool isProperNaturalPower() const;

    /**
     * Returns the highest degree of any variable in this polynomial expression
     */
    int getMaxDegree() const;

    /**
     * Returns a set of all ginac symbols that occur in this expression
     * (similar to collectVariableNames)
     */
    void collectVariables(ExprSymbolSet &res) const;

    /**
     * Convenience method for collectVariables
     */
    ExprSymbolSet getVariables() const;

    /**
     * Checks whether this expression contains a variable that satisfies the given predicate.
     * The predicate is called with a `const ExprSymbol &` as argument.
     */
    template <typename P>
    bool hasVariableWith(P predicate) const {
        struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
            SymbolVisitor(P predicate) : predicate(predicate) {}
            void visit(const ExprSymbol &sym) {
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
     * Returns true iff this Expression does not contain any variables, i.e.,
     * getVariables().size() == 0 (but more efficient)
     */
    bool hasNoVariables() const;

    /**
     * Returns true iff this Expression contains exactly one variable, i.e.,
     * getVariables().size() == 1 (but more efficient)
     */
    bool hasExactlyOneVariable() const;

    /**
     * Returns a variable that occurs in this Expression (given that there is one)
     */
    ExprSymbol getAVariable() const;

    /**
     * Returns true iff this Expression contains at most one variable, i.e.,
     * getVariables().size() <= 1 (but this methods is more efficient)
     */
    bool hasAtMostOneVariable() const;

    /**
     * Returns true iff this Expression contains at most one variable, i.e.,
     * getVariables().size() >= 2 (but this methods is more efficient)
     */
    bool hasAtLeastTwoVariables() const;

    /**
     * Returns an estimate of the exponent of the complexity class, e.g. "x^3" is 3, "x*y" is 2, "42" is 0 (constant)
     * @note this should be an OVER-approximation, but there are no guarantees as this is just a simple syntactic check!
     * @return the complexity, or ComplexExp for exponential, or ComplexNone for unknown complexity
     */
    Complexity getComplexity() const;

    /**
     * Converts this expression to a string by using GiNaC's operator<<
     */
    std::string toString() const;

    Expression lhs() const;
    Expression rhs() const;
    bool is_equal(const Expression &that) const;
    int degree(const ExprSymbol &var) const;
    int ldegree(const ExprSymbol &var) const;
    Expression coeff(const ExprSymbol &var, uint degree = 1) const;
    Expression lcoeff(const ExprSymbol &var, uint degree = 1) const;
    Expression expand() const;
    bool has(const Expression &pattern) const;
    bool info(unsigned i) const;
    bool is_zero() const;
    bool isSymbol() const;
    bool isNumeric() const;
    bool isPower() const;
    bool isMul() const;
    bool isAdd() const;
    bool isLess() const;
    bool isRelation() const;
    ExprSymbol toSymbol() const;
    GiNaC::numeric toNumeric() const;
    Expression op(uint i) const;
    uint nops() const;
    Expression subs(const ExprMap &map, uint options = 0) const;
    Expression subs(const Expression &eq, uint options = 0) const;
    void traverse(GiNaC::visitor & v) const;
    void swap(Expression &that);
    int compare(const Expression &that) const;
    bool is_polynomial(const ExprSymbol &var) const;
    Expression numer() const;
    Expression denom() const;
    bool match(const Expression &pattern) const;

    friend Expression operator-(const Expression &x);
    friend Expression operator-(const Expression &x, const Expression &y);
    friend Expression operator+(const Expression &x, const Expression &y);
    friend Expression operator*(const Expression &x, const Expression &y);
    friend Expression operator/(const Expression &x, const Expression &y);
    friend Expression operator<(const Expression &x, const Expression &y);
    friend Expression operator>(const Expression &x, const Expression &y);
    friend Expression operator<=(const Expression &x, const Expression &y);
    friend Expression operator>=(const Expression &x, const Expression &y);
    friend Expression operator!=(const Expression &x, const Expression &y);
    friend Expression operator^(const Expression &x, const Expression &y);
    friend std::ostream& operator<<(std::ostream &s, const Expression &e);
    friend Expression operator==(const Expression &x, const Expression &y);

private:
    GiNaC::ex ex;
    /**
     * Helper for getComplexity that operates recursively on the given term
     */
    static Complexity getComplexity(const Expression &term);

};

class ExprMap {
    friend class Expression;
public:
    ExprMap();
    ExprMap(const Expression &key, const Expression &val);
    Expression operator[](const Expression &key) const;
    void put(const Expression &key, const Expression &val);
    std::map<Expression, Expression, Expression_is_less>::const_iterator begin() const;
    std::map<Expression, Expression, Expression_is_less>::const_iterator end() const;
    std::map<Expression, Expression, Expression_is_less>::const_iterator find(const Expression &e) const;
    bool contains(const Expression &e) const;
    bool empty() const;

    friend bool operator<(const ExprMap &x, const ExprMap &y);

private:
    std::map<Expression, Expression, Expression_is_less> map;
    std::map<GiNaC::ex, GiNaC::ex, GiNaC::ex_is_less> ginacMap;

};

#endif // EXPRESSION_H
