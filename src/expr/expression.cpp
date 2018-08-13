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

#include "expression.h"

#include "expr/ginactoz3.h"
#include "z3/z3context.h"
#include "complexity.h"

using namespace std;


const ExprSymbol Expression::NontermSymbol = GiNaC::symbol("NONTERM");


Expression Expression::fromString(const string &s, const GiNaC::lst &variables) {
    auto containsRelations = [](const string &s) -> bool {
        return s.find_first_of("<>=") != string::npos;
    };

    if (!containsRelations(s)) {
        return Expression(GiNaC::ex(s,variables));
    }

    // The order is important to avoid parsing e.g. <= as <
    string ops[] = { "==", "!=", "<=", ">=", "<", ">", "=" };

    for (string op : ops) {
        string::size_type pos;
        if ((pos = s.find(op)) != string::npos) {
            string lhs = s.substr(0,pos);
            string rhs = s.substr(pos+op.length());

            if (containsRelations(lhs) || containsRelations(rhs)) {
                throw InvalidRelationalExpression("Multiple relational operators: "+s);
            }

            Expression lhsExpr = GiNaC::ex(lhs,variables);
            Expression rhsExpr = GiNaC::ex(rhs,variables);

            if (op == "<") return Expression(lhsExpr < rhsExpr);
            else if (op == ">") return Expression(lhsExpr > rhsExpr);
            else if (op == "<=") return Expression(lhsExpr <= rhsExpr);
            else if (op == ">=") return Expression(lhsExpr >= rhsExpr);
            else if (op == "!=") return Expression(lhsExpr != rhsExpr);
            else return Expression(lhsExpr == rhsExpr);
        }
    }
    unreachable();
}

void Expression::applySubs(const GiNaC::exmap &subs) {
    *this = this->subs(subs);
}

bool Expression::findAll(const GiNaC::ex &pattern, GiNaC::exset &found) const {
    bool anyFound = false;

    if (match(pattern)) {
        found.insert(*this);
        anyFound = true;
    }

    for (size_t i = 0; i < nops(); i++) {
        if (Expression(op(i)).findAll(pattern, found)) {
            anyFound = true;
        }
    }

    return anyFound;
}


bool Expression::equalsVariable(const GiNaC::symbol &var) const {
    return this->compare(var) == 0;
}


bool Expression::isNontermSymbol() const {
    return equalsVariable(NontermSymbol);
}


bool Expression::isLinear() const {
    // linear expressions are always polynomials
    if (!isPolynomial()) return false;

    // degree only works reliable on expanded expressions (despite the tutorial stating otherwise)
    Expression expanded = expand();

    // GiNaC does not provide an info flag for this, so we check the degree of every variable.
    // We also have to check if the coefficient contains variables,
    // e.g. y has degree 1 in x*y, but we don't consider x*y to be linear.
    for (const ExprSymbol &var : getVariables()) {
        int deg = expanded.degree(var);
        if (deg > 1 || deg < 0) {
            return false;
        }

        if (deg == 1) {
            if (!expanded.coeff(var,deg).info(GiNaC::info_flags::numeric)) {
                return false;
            }
        }
    }
    return true;
}


bool Expression::isPolynomial() const {
    return this->info(GiNaC::info_flags::polynomial);
}


bool Expression::isPolynomialWithIntegerCoeffs() const {
    return this->info(GiNaC::info_flags::integer_polynomial);
}


bool Expression::isIntegerConstant() const {
    return this->info(GiNaC::info_flags::integer);
}


bool Expression::isRationalConstant() const {
    return this->info(GiNaC::info_flags::rational);
}


bool Expression::isProperRational() const {
    return this->info(GiNaC::info_flags::rational)
           && !this->info(GiNaC::info_flags::integer);
}


bool Expression::isProperNaturalPower() const {
    if (!GiNaC::is_a<GiNaC::power>(*this)) {
        return false;
    }

    GiNaC::ex power = this->op(1);
    if (!power.info(GiNaC::info_flags::integer)) {
        return false;
    }

    return GiNaC::ex_to<GiNaC::numeric>(power) > GiNaC::numeric(1);
}


int Expression::getMaxDegree() const {
    assert(isPolynomial());
    Expression expanded = expand();

    int res = 0;
    for (const auto &var : getVariables()) {
        res = std::max(res, expanded.degree(var));
    }
    return res;
}


void Expression::collectVariables(ExprSymbolSet &res) const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(ExprSymbolSet &t) : target(t) {}
        void visit(const GiNaC::symbol &sym) {
            if (sym != NontermSymbol) target.insert(sym);
        }
    private:
        ExprSymbolSet &target;
    };

    SymbolVisitor v(res);
    traverse(v);
}


ExprSymbolSet Expression::getVariables() const {
    ExprSymbolSet res;
    collectVariables(res);
    return res;
}


bool Expression::hasNoVariables() const {
    return !hasVariableWith([](const ExprSymbol &) { return true; });
}


bool Expression::hasExactlyOneVariable() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        void visit(const GiNaC::symbol &var) {
            if (foundVar == nullptr) {
                foundVar = &var;
                exactlyOneVar = true;

            } else if (exactlyOneVar && var != *foundVar) {
                exactlyOneVar = false;
            }
        }
        bool result() const {
            return exactlyOneVar;
        }
    private:
        bool exactlyOneVar = false;
        const GiNaC::symbol *foundVar = nullptr;
    };

    SymbolVisitor visitor;
    traverse(visitor);
    return visitor.result();
}


ExprSymbol Expression::getAVariable() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        void visit(const GiNaC::symbol &var) {
            variable = var;
        }
        ExprSymbol result() const {
            return variable;
        }
    private:
        ExprSymbol variable;
    };

    SymbolVisitor visitor;
    traverse(visitor);
    return visitor.result();
}


bool Expression::hasAtMostOneVariable() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        void visit(const GiNaC::symbol &var) {
            if (foundVar == nullptr) {
                foundVar = &var;

            } else if (var != *foundVar) {
                atMostOneVar = false;
            }
        }
        bool result() const {
            return atMostOneVar;
        }
    private:
        bool atMostOneVar = true;
        const GiNaC::symbol *foundVar = nullptr;
    };

    SymbolVisitor visitor;
    traverse(visitor);
    return visitor.result();
}


bool Expression::hasAtLeastTwoVariables() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        void visit(const GiNaC::symbol &var) {
            if (foundVar == nullptr) {
                foundVar = &var;

            } else if (var != *foundVar) {
                atLeastTwoVars = true;
            }
        }
        bool result() const {
            return atLeastTwoVars;
        }
    private:
        bool atLeastTwoVars = false;
        const GiNaC::symbol *foundVar = nullptr;
    };

    SymbolVisitor visitor;
    traverse(visitor);
    return visitor.result();
}


z3::expr Expression::toZ3(Z3Context &context, bool useReals) const {
    return GinacToZ3::convert(*this, context, useReals);
}


Complexity Expression::getComplexity(const GiNaC::ex &term) {
    using namespace GiNaC;

    //traverse the expression
    if (is_a<numeric>(term)) {
        numeric num = ex_to<numeric>(term);
        assert(num.is_integer() || num.is_real());
        //both for positive and negative constants, as we want to over-approximate the complexity! (e.g. A-B is O(n))
        return Complexity::Const;

    } else if (is_a<power>(term)) {
        assert(term.nops() == 2);

        // If the exponent is at least polynomial (non-constant), complexity might be exponential
        if (getComplexity(term.op(1)) > Complexity::Const) {
            const ex &base = term.op(0);
            if (base.is_zero() || base.compare(1) == 0 || base.compare(-1) == 0) {
                return Complexity::Const;
            }
            return Complexity::Exp;

        // Otherwise the complexity is polynomial, if the exponent is nonnegative
        } else {
            if (!is_a<numeric>(term.op(1))) {
                return Complexity::Unknown;
            }
            numeric numexp = ex_to<numeric>(term.op(1));
            if (!numexp.is_nonneg_integer()) {
                return Complexity::Unknown;
            }

            Complexity base = getComplexity(term.op(0));
            int exp = numexp.to_int();
            return base ^ exp;
        }

    } else if (is_a<mul>(term)) {
        assert(term.nops() > 0);
        Complexity cpx = getComplexity(term.op(0));
        for (int i=1; i < term.nops(); ++i) {
            cpx = cpx * getComplexity(term.op(i));
        }
        return cpx;

    } else if (is_a<add>(term)) {
        assert(term.nops() > 0);
        Complexity cpx = getComplexity(term.op(0));
        for (int i=1; i < term.nops(); ++i) {
            cpx = cpx + getComplexity(term.op(i));
        }
        return cpx;

    } else if (is_a<symbol>(term)) {
        return (term.compare(Expression::NontermSymbol) == 0) ? Complexity::Nonterm : Complexity::Poly(1);
    }

    //unknown expression type (e.g. relational)
    debugWarn("Expression: getComplexity called on unknown expression type");
    return Complexity::Unknown;
}


Complexity Expression::getComplexity() const {
    if (isNontermSymbol()) {
        return Complexity::Nonterm;
    }

    Expression simple = expand(); // multiply out
    return getComplexity(simple);
}


string Expression::toString() const {
    stringstream ss;
    ss << *this;
    return ss.str();
}
