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

#include "z3/z3context.h"
#include "complexity.h"

using namespace std;


const ExprSymbol Expression::InfSymbol = GiNaC::symbol("INF");


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


bool Expression::isInfty() const {
    //trivial cases
    if (degree(InfSymbol) == 0) return false;
    if (equalsVariable(InfSymbol)) return true;

    //check if INF is used in a simple polynomial manner with positive coefficients
    int d = degree(InfSymbol);
    if (coeff(InfSymbol, d).info(GiNaC::info_flags::positive)) {
        return true;
    }

    //we do not know if this expression is INF or not
    debugWarn("Expression: unsure if INF: " << this);
    return false;
}


bool Expression::isLinear(const GiNaC::lst &vars) const {
    //this must be a polynomial expression
    if (!is_polynomial(vars)) return false;

    for (auto var : vars) {
        int deg = degree(var);
        if (deg > 1 || deg < 0) return false; //nonlinear

        //coefficient must not contain any variables (e.g. x*y is nonlinear)
        if (deg == 1) {
            if (!GiNaC::is_a<GiNaC::numeric>(coeff(var,deg))) return false;
        }
    }
    return true;
}


bool Expression::isPolynomialWithin(const ExprSymbolSet &vars) const {
    return std::all_of(vars.begin(), vars.end(), [&](const ExprSymbol &var){ return is_polynomial(var); });
}


bool Expression::isPolynomial() const {
    return isPolynomialWithin(getVariables());
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
    assert(info(GiNaC::info_flags::polynomial));

    int res = 0;
    for (auto var : getVariables()) {
        if (is_polynomial(var)) {
            int deg = degree(var);

            if (deg > res) {
                res = deg;
            }
        }
    }

    return res;
}


int Expression::getMaxDegree(const GiNaC::lst &vars) const {
    assert(this->is_polynomial(vars));

    int res = 0;
    for (auto var : vars) {
        int deg = degree(var);
        if (deg > res) res = deg;
    }
    return res;
}


void Expression::collectVariableNames(set<string> &res) const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(set<string> &t) : target(t) {}
        void visit(const GiNaC::symbol &sym) {
            if (sym != InfSymbol) target.insert(sym.get_name());
        }
    private:
        set<string> &target;
    };

    SymbolVisitor v(res);
    traverse(v);
}


void Expression::collectVariables(ExprSymbolSet &res) const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(ExprSymbolSet &t) : target(t) {}
        void visit(const GiNaC::symbol &sym) {
            if (sym != InfSymbol) target.insert(sym);
        }
    private:
        ExprSymbolSet &target;
    };

    SymbolVisitor v(res);
    traverse(v);
}



set<string> Expression::getVariableNames() const {
    set<string> res;
    collectVariableNames(res);
    return res;
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


z3::expr Expression::toZ3(Z3Context &context, GinacToZ3::Settings cfg) const {
    return GinacToZ3::convert(*this, context, cfg);
}


Expression Expression::removedExponents() const {
    GiNaC::exmap subsMap;
    auto vars = getVariables();

    int label=0;
    for (ExprSymbol var : vars) {
        subsMap[GiNaC::pow(var,GiNaC::wild(label++))] = var;
    }
    return subs(subsMap);
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
        return (term.compare(Expression::InfSymbol) == 0) ? Complexity::Nonterm : Complexity::Poly(1);
    }

    //unknown expression type (e.g. relational)
    debugWarn("Expression: getComplexity called on unknown expression type");
    return Complexity::Unknown;
}


Complexity Expression::getComplexity() const {
    Expression simple = expand();
    return getComplexity(simple);
}


GiNaC::ex Expression::simplifyForComplexity(GiNaC::ex term) {
    if (GiNaC::is_a<GiNaC::power>(term)) {
        assert(term.nops() == 2);
        if (GiNaC::is_a<GiNaC::numeric>(term.op(0))) {
            GiNaC::numeric num = GiNaC::ex_to<GiNaC::numeric>(term.op(0));
            if (num.compare(1) == 1) {
                term = GiNaC::power(2,term.op(1));
           }
            if (!GiNaC::is_a<GiNaC::numeric>(term.op(1))) {
                term = GiNaC::power(term.op(0),simplifyForComplexity(term.op(1))); //simplify non-numeric exponent
            }
        }
        else {
            term = GiNaC::power(simplifyForComplexity(term.op(0)),term.op(1));
        }
    } else if (GiNaC::is_a<GiNaC::numeric>(term)) {
        GiNaC::numeric num = GiNaC::ex_to<GiNaC::numeric>(term);
        if (num.is_positive()) {
            term = 1;
        }
    } else if (GiNaC::is_a<GiNaC::mul>(term)) {
        GiNaC::ex res = 1;
        for (int i=0; i < term.nops(); ++i) {
            res = res * simplifyForComplexity(term.op(i));
        }
        term = res;
    } else if (GiNaC::is_a<GiNaC::add>(term)) {
        GiNaC::ex res = 0;
        for (int i=0; i < term.nops(); ++i) {
            if (!GiNaC::is_a<GiNaC::numeric>(term.op(i))) {
                res = res + simplifyForComplexity(term.op(i));
            }
        }
        term = res;
    } else if (!GiNaC::is_a<GiNaC::symbol>(term)){
        throw UnknownComplexityClassException("Unknown GiNaC type");
    }
    return term;
}


Expression Expression::calcComplexityClass() const {
    GiNaC::ex term = this->expand();
    return simplifyForComplexity(term);
}

