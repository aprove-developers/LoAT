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

#include "global.h"

#include <ginac/ginac.h>
#include <z3++.h>
#include <string>
#include <vector>

#include "util/exceptions.h"
#include "ginactoz3.h"
#include "complexity.h"


class Z3Context;
class Expression;

//Useful typedefs for readability

using ExprSymbol = GiNaC::symbol; // TODO: rename to just Symbol (and SymbolSet, SymbolMap)?
using ExprList = GiNaC::lst;

using ExprSymbolSet = std::set<ExprSymbol, GiNaC::ex_is_less>;
using ExpressionSet = std::set<Expression, GiNaC::ex_is_less>

template <typename T>
using ExprSymbolMap = std::map<ExprSymbol, T, GiNaC::ex_is_less>;

// TODO: Add alias for GiNaC::exmap (Substitution / Subs), or even a class?

//typedef GiNaC::symbol ExprSymbol;
//typedef GiNaC::lst ExprList;
//typedef std::set<ExprSymbol, GiNaC::ex_is_less> ExprSymbolSet;
//typedef std::set<Expression, GiNaC::ex_is_less> ExpressionSet;




/**
 * Class for arithmetic expressions, can be converted to a z3 expression.
 * Essentially a wrapper around GiNaC::ex which provides additional functionality.
 */
class Expression : public GiNaC::ex {
public:
    static const ExprSymbol InfSymbol; // special symbol "INF" to be used within cost expressions

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
    Expression(const GiNaC::ex &other) : GiNaC::ex(other) {
#ifdef DEBUG_EXPRESSION
        //build string representation to be used in debugger
        std::stringstream ss;
        print(GiNaC::print_dflt(ss));
        string_rep = ss.str();
#endif
    }

    /**
     * Applies a substitution. Like GiNaC::subs, but assigns the result to this expression.
     */
    void applySubs(const GiNaC::exmap &subs);

    /**
     * Version of ex::find() that searches also in subexpressions of a match
     */
    bool findAll(const GiNaC::ex &pattern, GiNaC::exset &found) const;

    /**
     * Returns true iff this expression is the given variable (up to trivial arithmetic, which is resolved by GiNaC)
     */
    bool equalsVariable(const GiNaC::symbol &var) const;

    /**
     * Checks if this expression represents infinity, i.e. if it is (equivalent to) the INF-symbol.
     * @note Only a heuristic check, returns false if unsure.
     */
    bool isInfty() const;

    // TODO: replace by isLinear and isLinearWithin(ExprSymbolSet)
    // TODO: This is probably cleaner than the getGinacVarList() everywhere...
    /**
     * Returns true iff this expression is linear in the given variables
     */
    bool isLinear(const GiNaC::lst &vars) const;

    /**
     * Returns true iff this expression is polynomial in the given variables
     */
    bool isPolynomialWithin(const ExprSymbolSet &vars) const;

    // TODO: Check if performance is ok if we use this instead of is_polynomial
    /**
     * Returns true iff this expression is polynomial in the given variables
     */
    bool isPolynomial() const;

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
     * Returns the highest degree of any of the given variables in this polynomial expression
     * @note this must be polynomial in vars!
     */
    int getMaxDegree(const GiNaC::lst &vars) const;

    // TODO: Deprecate collectVariableNames. Different symbols with same name should be treated differently
    // TODO: (and they are treated differently when we use GiNaC::symbol and ExprSymbolSet instead of their names).
    /**
     * Adds all variable names that occur in this expression to the given set
     */
    void collectVariableNames(std::set<std::string> &res) const;

    /**
     * Convenience method for collectVariableNames
     */
    std::set<std::string> getVariableNames() const;

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
     * Converts this term from a GiNaC::ex to a Z3 expression, see GinacToZ3
     * @return newly created z3 expression
     */
    z3::expr toZ3(Z3Context &context, GinacToZ3::Settings cfg = {}) const;

    /**
     * Return new expression without any powers of symbols, e.g. x^2 * y^x --> x * y ("5^x" is kept)
     */
    Expression removedExponents() const;

    /**
     * Returns an estimate of the exponent of the complexity class, e.g. "x^3" is 3, "x*y" is 2, "42" is 0 (constant)
     * @note this should be an OVER-approximation, but there are no guarantees as this is just a simple syntactic check!
     * @return the complexity, or ComplexExp for exponential, or ComplexNone for unknown complexity
     */
    Complexity getComplexity() const;

    /**
     * Tries to calculate the complexity class of this expression, returns i.e. y^2 for (2*y*y+y)
     * @note this does currently only handle simple cases and is thus not reliable!
     */
    Expression calcComplexityClass() const;
    EXCEPTION(UnknownComplexityClassException,CustomException);


private:
    /**
     * Helper to remove coefficients for complexity class
     */
    static GiNaC::ex simplifyForComplexity(GiNaC::ex term);

    /**
     * Helper for getComplexity that operates recursively on the given term
     */
    static Complexity getComplexity(const GiNaC::ex &term);


private:
#ifdef DEBUG_EXPRESSION
    std::string string_rep;
#endif
};

#endif // EXPRESSION_H
