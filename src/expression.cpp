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
#include "debug.h"

#include <string>
#include <map>
#include <limits>
#include <functional>

#include <z3++.h>

#include "itrs.h"
#include "z3toolbox.h"


using namespace std;

//constants to make Complexity easily comparable (now even more ugly thanks to floats)
const Complexity Expression::ComplexExp = 10000;
const Complexity Expression::ComplexExpMore = 20000;
const Complexity Expression::ComplexInfty = 99999;
const Complexity Expression::ComplexNone = -42;
const ExprSymbol Expression::Infty = GiNaC::symbol("INF");

ostream& operator<<(ostream &s, const Complexity &cpx) {
    s << cpx.toString();
    return s;
}

z3::expr Expression::ginacToZ3(const GiNaC::ex &term, Z3VariableContext &context, bool fresh, bool reals) {
    if (GiNaC::is_a<GiNaC::add>(term)) {
        assert(term.nops() > 0);
        z3::expr res = ginacToZ3(term.op(0),context,fresh,reals);
        for (int i=1; i < term.nops(); ++i) {
            res = res + ginacToZ3(term.op(i),context,fresh,reals);
        }
        return res;
    }
    else if (GiNaC::is_a<GiNaC::mul>(term)) {
        assert(term.nops() > 0);
        z3::expr res = ginacToZ3(term.op(0),context,fresh,reals);
        for (int i=1; i < term.nops(); ++i) {
            res = res * ginacToZ3(term.op(i),context,fresh,reals);
        }
        return res;
    }
    else if (GiNaC::is_a<GiNaC::power>(term)) {
        assert(term.nops() == 2);
        if (GiNaC::is_a<GiNaC::numeric>(term.op(1))) {
            //rewrite power as multiplication if possible, which z3 can handle much better
            GiNaC::numeric num = GiNaC::ex_to<GiNaC::numeric>(term.op(1));
            if (num.is_integer() && num.is_positive() && num.to_int() <= Z3_MAX_EXPONENT) {
                int exp = num.to_int();
                z3::expr base = ginacToZ3(term.op(0),context,fresh,reals);
                z3::expr res = base;
                while (--exp > 0) res = res * base;
                return res;
            }
        }
        //use z3 power as fallback (only poorly supported)
        return z3::pw(ginacToZ3(term.op(0),context,fresh,reals),ginacToZ3(term.op(1),context,fresh,reals));
    }
    else if (GiNaC::is_a<GiNaC::numeric>(term)) {
        const GiNaC::numeric &num = GiNaC::ex_to<GiNaC::numeric>(term);
        assert(num.is_integer() || num.is_real());
        try {
            if (num.is_integer()) {
                return (reals) ? context.real_val(num.to_int(),1) : context.int_val(num.to_int());
            } else {
                return context.real_val(num.numer().to_int(),num.denom().to_int());
            }
        } catch (...) { throw GinacZ3ConversionError("Invalid numeric constant (value too large)"); }
    }
    else if (GiNaC::is_a<GiNaC::symbol>(term)) {
        Z3VariableContext::VariableType type = (reals) ? Z3VariableContext::Real : Z3VariableContext::Integer;
        const GiNaC::symbol &sym = GiNaC::ex_to<GiNaC::symbol>(term);
        return fresh ? context.getFreshVariable(sym.get_name(),type) : context.getVariable(sym.get_name(),type);
    }
    else if (GiNaC::is_a<GiNaC::relational>(term)) {
        assert(term.nops() == 2);
        z3::expr a = ginacToZ3(term.op(0),context,fresh,reals);
        z3::expr b = ginacToZ3(term.op(1),context,fresh,reals);
        if (term.info(GiNaC::info_flags::relation_equal)) return a == b;
        if (term.info(GiNaC::info_flags::relation_not_equal)) return a != b;
        if (term.info(GiNaC::info_flags::relation_less)) return a < b;
        if (term.info(GiNaC::info_flags::relation_less_or_equal)) return a <= b;
        if (term.info(GiNaC::info_flags::relation_greater)) return a > b;
        if (term.info(GiNaC::info_flags::relation_greater_or_equal)) return a >= b;
        assert(false);
    }

    string errormsg;
    stringstream ss(errormsg);
    ss << "ERROR: GiNaC type not implemented for term: " << term << endl;
    throw GinacZ3ConversionError(ss.str());
}



Expression Expression::fromString(const string &s, const GiNaC::lst &variables) {
    auto containsNoRelations = [](const string &s) -> bool {
        return s.find(">") == string::npos && s.find("<") == string::npos && s.find("=") == string::npos;
    };

    if (containsNoRelations(s)) {
        return Expression(GiNaC::ex(s,variables));
    }

    for (string op : {"<",">","==","!=","="}) {
        string::size_type pos;
        if ((pos = s.find(op)) != string::npos) {
            string lhs = s.substr(0,pos);

            if (op == "<" && pos+1 < s.length() && s[pos+1] == '=') op = "<=";
            if (op == ">" && pos+1 < s.length() && s[pos+1] == '=') op = ">=";

            string rhs = s.substr(pos+op.length());
            if (!containsNoRelations(rhs)) throw InvalidRelationalExpression("Multiple relational operators: "+s);

            Expression lhsExpr = fromString(lhs,variables);
            Expression rhsExpr = fromString(rhs,variables);

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
    if (degree(Infty) == 0) return false;
    if (equalsVariable(Infty)) return true;

    //check if INF is used in a simple polynomial manner with positive coefficients
    int d = degree(Infty);
    GiNaC::ex c = coeff(Infty,d);
    if (GiNaC::is_a<GiNaC::numeric>(c)) {
        if (GiNaC::ex_to<GiNaC::numeric>(c).is_positive()) {
            return true;
        }
    }

    //we do not know if this expression is INF or not
    debugOther("WARNING: Expression: unsure if INF: " << this);
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
            if (sym != Infty) target.insert(sym.get_name());
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
            if (sym != Infty) target.insert(sym);
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
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(bool &b)
            : noVars(b) {
        }
        void visit(const GiNaC::symbol &var) {
            noVars = false;
        }
    private:
        bool &noVars;
    };

    bool noVars = true;

    SymbolVisitor visitor(noVars);
    traverse(visitor);

    return noVars;
}


bool Expression::hasExactlyOneVariable() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(bool &b)
            : exactlyOneVar(b), foundVar(nullptr) {
        }
        void visit(const GiNaC::symbol &var) {
            if (foundVar == nullptr) {
                foundVar = &var;
                exactlyOneVar = true;

            } else {
                if (var != *foundVar) {
                    exactlyOneVar = false;
                }
            }
        }
    private:
        bool &exactlyOneVar;
        const GiNaC::symbol *foundVar;
    };

    bool exactlyOneVar = false;

    SymbolVisitor visitor(exactlyOneVar);
    traverse(visitor);

    return exactlyOneVar;
}


ExprSymbol Expression::getAVariable() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(GiNaC::symbol &variable)
            : variable(variable) {
        }
        void visit(const GiNaC::symbol &var) {
            variable = var;
        }
    private:
        GiNaC::symbol &variable;
    };

    ExprSymbol variable;

    SymbolVisitor visitor(variable);
    traverse(visitor);

    return variable;
}


bool Expression::hasAtMostOneVariable() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(bool &b)
            : atMostOneVar(b), foundVar(nullptr) {
        }
        void visit(const GiNaC::symbol &var) {
            if (foundVar == nullptr) {
                foundVar = &var;

            } else {
                if (var != *foundVar) {
                    atMostOneVar = false;
                }
            }
        }
    private:
        bool &atMostOneVar;
        const GiNaC::symbol *foundVar;
    };

    bool atMostOneVar = true;

    SymbolVisitor visitor(atMostOneVar);
    traverse(visitor);

    return atMostOneVar;
}


bool Expression::hasAtLeastTwoVariables() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(bool &b)
            : atLeastTwoVars(b), foundVar(nullptr) {
        }
        void visit(const GiNaC::symbol &var) {
            if (foundVar == nullptr) {
                foundVar = &var;

            } else if (var != *foundVar) {
                atLeastTwoVars = true;
            }
        }
    private:
        bool &atLeastTwoVars;
        const GiNaC::symbol *foundVar;
    };

    bool atLeastTwoVars = false;

    SymbolVisitor visitor(atLeastTwoVars);
    traverse(visitor);

    return atLeastTwoVars;
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
    //define some helpers
    using namespace placeholders;
    auto cpxop = [](Complexity a, Complexity b, std::function<Complexity(Complexity,Complexity)> op) -> Complexity {
        if (a == ComplexNone || b == ComplexNone) return ComplexNone;
        if (a == ComplexInfty || b == ComplexInfty) return ComplexInfty;
        return (a==ComplexExp || b==ComplexExp) ? ComplexExp : op(a,b);
    };
    auto cpxmul = std::bind(cpxop,_1,_2,[](Complexity a, Complexity b){ return a*b; });
    auto cpxadd = std::bind(cpxop,_1,_2,[](Complexity a, Complexity b){ return a+b; });

    //traverse the expression
    if (GiNaC::is_a<GiNaC::numeric>(term)) {
        GiNaC::numeric num = GiNaC::ex_to<GiNaC::numeric>(term);
        assert(num.is_integer() || num.is_real());
        return 0; //both for positive and negative constants, as we want to over-approximate the complexity! (e.g. A-B is O(n))
    }
    else if (GiNaC::is_a<GiNaC::power>(term)) {
        assert(term.nops() == 2);
        if (getComplexity(term.op(1)) > 0) {
            return (term.op(0).is_zero() || term.op(0).compare(1)==0 || term.op(0).compare(-1)==0) ? 0 : ComplexExp;
        } else {
            if (!GiNaC::is_a<GiNaC::numeric>(term.op(1))) return ComplexNone; //unknown
            GiNaC::numeric numexp = GiNaC::ex_to<GiNaC::numeric>(term.op(1));
            if (!numexp.is_nonneg_integer()) return ComplexNone; //unknown (negative/float exponent)
            Complexity base = getComplexity(term.op(0));
            int exp = numexp.to_int();
            return (base == -1 || exp < 0) ? ComplexNone : cpxmul(base,exp);
        }
    }
    else if (GiNaC::is_a<GiNaC::mul>(term)) {
        assert(term.nops() > 0);
        Complexity cpx = getComplexity(term.op(0));
        for (int i=1; cpx != ComplexNone && i < term.nops(); ++i) {
            cpx = cpxadd(cpx,getComplexity(term.op(i)));
        }
        return cpx;
    }
    else if (GiNaC::is_a<GiNaC::add>(term)) {
        assert(term.nops() > 0);
        Complexity cpx = getComplexity(term.op(0));
        for (int i=1; i < term.nops(); ++i) {
            cpx = max(cpx,getComplexity(term.op(i)));
        }
        return cpx;
    }
    else if (GiNaC::is_a<GiNaC::symbol>(term)) {
        return (term.compare(Expression::Infty) == 0) ? ComplexInfty : 1;
    }

    //unknown expression type (e.g. relational)
    return ComplexNone;
}


Complexity Expression::getComplexity() const {
    Expression simple = expand();
    return getComplexity(simple);
}


string Expression::complexityString(Complexity complexity) {
    if (complexity == 0) return "const";
    if (complexity == ComplexNone) return "none";
    if (complexity == ComplexExp) return "EXP";
    if (complexity == ComplexExpMore) return "EXP NESTED";
    if (complexity == ComplexInfty) return "INF";
    stringstream ss;
    ss << "n^" << complexity;
    return ss.str();
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

