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

#include "expression.hpp"
#include "complexity.hpp"
#include <sstream>

using namespace std;

bool Expr_is_less::operator()(const Expr &lh, const Expr &rh) const {
     return lh.compare(rh) < 0;
}

std::ostream& operator<<(std::ostream &s, const ExprMap &map) {
    if (map.empty()) {
        s << "{}";
    } else {
        s << "{";
        bool fst = true;
        for (const auto &p: map) {
            if (!fst) {
                s << ", ";
            } else {
                fst = false;
            }
            s << p.first << ": " << p.second;
        }
    }
    return s;
}

const Var Expr::NontermSymbol = GiNaC::symbol("NONTERM");

void Expr::applySubs(const ExprMap &subs) {
    this->ex = this->ex.subs(subs.ginacMap);
}

bool Expr::findAll(const Expr &pattern, ExprSet &found) const {
    bool anyFound = false;

    if (match(pattern)) {
        found.insert(*this);
        anyFound = true;
    }

    for (size_t i = 0; i < arity(); i++) {
        if (op(i).findAll(pattern, found)) {
            anyFound = true;
        }
    }

    return anyFound;
}


bool Expr::equals(const Var &var) const {
    return this->compare(var) == 0;
}


bool Expr::isNontermSymbol() const {
    return equals(NontermSymbol);
}


bool Expr::isLinear(const option<VarSet> &vars) const {
    VarSet theVars = vars ? vars.get() : this->vars();
    // linear expressions are always polynomials
    if (!isPoly()) return false;

    // degree only works reliable on expanded expressions (despite the tutorial stating otherwise)
    Expr expanded = expand();

    // GiNaC does not provide an info flag for this, so we check the degree of every variable.
    // We also have to check if the coefficient contains variables,
    // e.g. y has degree 1 in x*y, but we don't consider x*y to be linear.
    for (const Var &var : theVars) {
        int deg = expanded.degree(var);
        if (deg > 1 || deg < 0) {
            return false;
        }

        if (deg == 1) {
            VarSet coefficientVars = expanded.coeff(var,deg).vars();
            for (const Var &e: coefficientVars) {
                if (theVars.find(e) != theVars.end()) {
                    return false;
                }
            }
        }
    }
    return true;
}


bool Expr::isPoly() const {
    return ex.info(GiNaC::info_flags::polynomial);
}

bool Expr::isPoly(const Var &n) const {
    return ex.is_polynomial(n);
}


bool Expr::isIntPoly() const {
    return ex.info(GiNaC::info_flags::integer_polynomial);
}


bool Expr::isInt() const {
    return ex.info(GiNaC::info_flags::integer);
}


bool Expr::isRationalConstant() const {
    return ex.info(GiNaC::info_flags::rational);
}


bool Expr::isNonIntConstant() const {
    return ex.info(GiNaC::info_flags::rational)
           && !ex.info(GiNaC::info_flags::integer);
}


bool Expr::isNaturalPow() const {
    if (!this->isPow()) {
        return false;
    }

    Expr power = this->op(1);
    if (!power.isInt()) {
        return false;
    }

    return power.toNum() > 1;
}


int Expr::maxDegree() const {
    assert(isPoly());
    Expr expanded = expand();

    int res = 0;
    for (const auto &var : vars()) {
        res = std::max(res, expanded.degree(var));
    }
    return res;
}


void Expr::collectVars(VarSet &res) const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        SymbolVisitor(VarSet &t) : target(t) {}
        void visit(const GiNaC::symbol &sym) {
            if (sym != NontermSymbol) target.insert(sym);
        }
    private:
        VarSet &target;
    };

    SymbolVisitor v(res);
    traverse(v);
}


VarSet Expr::vars() const {
    VarSet res;
    collectVars(res);
    return res;
}


bool Expr::isGround() const {
    return !hasVarWith([](const Var &) { return true; });
}


bool Expr::isUnivariate() const {
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


Var Expr::someVar() const {
    struct SymbolVisitor : public GiNaC::visitor, public GiNaC::symbol::visitor {
        void visit(const GiNaC::symbol &var) {
            variable = var;
        }
        Var result() const {
            return variable;
        }
    private:
        Var variable;
    };

    SymbolVisitor visitor;
    traverse(visitor);
    return visitor.result();
}


bool Expr::isNotMultivariate() const {
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


bool Expr::isMultivariate() const {
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


Complexity Expr::toComplexity(const Expr &term) {

    //traverse the expression
    if (term.isRationalConstant()) {
        GiNaC::numeric num = term.toNum();
        assert(num.is_integer() || num.is_real());
        //both for positive and negative constants, as we want to over-approximate the complexity! (e.g. A-B is O(n))
        return Complexity::Const;

    } else if (term.isPow()) {
        assert(term.arity() == 2);

        // If the exponent is at least polynomial (non-constant), complexity might be exponential
        if (toComplexity(term.op(1)) > Complexity::Const) {
            const Expr &base = term.op(0);
            if (base.isZero() || base.compare(1) == 0 || base.compare(-1) == 0) {
                return Complexity::Const;
            }
            return Complexity::Exp;

        // Otherwise the complexity is polynomial, if the exponent is nonnegative
        } else {
            if (!term.op(1).isRationalConstant()) {
                return Complexity::Unknown;
            }
            GiNaC::numeric numexp = term.op(1).toNum();
            if (!numexp.is_nonneg_integer()) {
                return Complexity::Unknown;
            }

            Complexity base = toComplexity(term.op(0));
            int exp = numexp.to_int();
            return base ^ exp;
        }

    } else if (term.isMul()) {
        assert(term.arity() > 0);
        Complexity cpx = toComplexity(term.op(0));
        for (unsigned int i=1; i < term.arity(); ++i) {
            cpx = cpx * toComplexity(term.op(i));
        }
        return cpx;

    } else if (term.isAdd()) {
        assert(term.arity() > 0);
        Complexity cpx = toComplexity(term.op(0));
        for (unsigned int i=1; i < term.arity(); ++i) {
            cpx = cpx + toComplexity(term.op(i));
        }
        return cpx;

    } else if (term.isVar()) {
        return (term.compare(Expr::NontermSymbol) == 0) ? Complexity::Nonterm : Complexity::Poly(1);
    }

    //unknown expression type (e.g. relational)
    return Complexity::Unknown;
}


Complexity Expr::toComplexity() const {
    if (isNontermSymbol()) {
        return Complexity::Nonterm;
    }

    Expr simple = expand(); // multiply out
    return toComplexity(simple);
}


string Expr::toString() const {
    stringstream ss;
    ss << *this;
    return ss.str();
}

bool Expr::equals(const Expr &that) const {
    return ex.is_equal(that.ex);
}

int Expr::degree(const Var &var) const {
    return ex.degree(var);
}

int Expr::ldegree(const Var &var) const {
    return ex.ldegree(var);
}

Expr Expr::coeff(const Var &var, int degree) const {
    return ex.coeff(var, degree);
}

Expr Expr::lcoeff(const Var &var) const {
    return ex.lcoeff(var);
}

Expr Expr::expand() const {
    return ex.expand();
}

bool Expr::has(const Expr &pattern) const {
    return ex.has(pattern.ex);
}

bool Expr::isZero() const {
    return ex.is_zero();
}

bool Expr::isVar() const {
    return ex.info(GiNaC::info_flags::symbol);
}

bool Expr::isPow() const {
    return GiNaC::is_a<GiNaC::power>(ex);
}

bool Expr::isMul() const {
    return GiNaC::is_a<GiNaC::mul>(ex);
};

bool Expr::isAdd() const {
    return GiNaC::is_a<GiNaC::add>(ex);
}

Var Expr::toVar() const {
    return GiNaC::ex_to<GiNaC::symbol>(ex);
}

GiNaC::numeric Expr::toNum() const {
    return GiNaC::ex_to<GiNaC::numeric>(ex);
}

Expr Expr::op(uint i) const {
    return ex.op(i);
}

size_t Expr::arity() const {
    return ex.nops();
}

Expr Expr::subs(const ExprMap &map) const {
    return ex.subs(map.ginacMap);
}

Expr Expr::replace(const ExprMap &patternMap) const {
    return ex.subs(patternMap.ginacMap, GiNaC::subs_options::algebraic);
}

void Expr::traverse(GiNaC::visitor &v) const {
    ex.traverse(v);
}

int Expr::compare(const Expr &that) const {
    return ex.compare(that.ex);
}

Expr Expr::numerator() const {
    return ex.numer();
}

Expr Expr::denominator() const {
    return ex.denom();
}

bool Expr::match(const Expr &pattern) const {
    return ex.match(pattern.ex);
}

Expr operator-(const Expr &x) {
    return -x.ex;
}

Expr operator-(const Expr &x, const Expr &y) {
    return x.ex - y.ex;
}

Expr operator+(const Expr &x, const Expr &y) {
    return x.ex + y.ex;
}

Expr operator*(const Expr &x, const Expr &y) {
    return x.ex * y.ex;
}

Expr operator/(const Expr &x, const Expr &y) {
    return x.ex / y.ex;
}

Expr operator^(const Expr &x, const Expr &y) {
    return GiNaC::pow(x.ex, y.ex);
}

Expr Expr::wildcard(uint label) {
    return GiNaC::wild(label);
}


Rel::Rel(const Expr &lhs, Operator op, const Expr &rhs): l(lhs), r(rhs), op(op) { }

Rel operator<(const Expr &x, const Expr &y) {
    return Rel(x.ex, Rel::lt, y.ex);
}

Rel operator>(const Expr &x, const Expr &y) {
    return Rel(x.ex, Rel::gt, y.ex);
}

Rel operator<=(const Expr &x, const Expr &y) {
    return Rel(x.ex, Rel::leq, y.ex);
}

Rel operator>=(const Expr &x, const Expr &y) {
    return Rel(x.ex, Rel::geq, y.ex);
}

Rel operator!=(const Expr &x, const Expr &y) {
    return Rel(x.ex, Rel::neq, y.ex);
}

Rel operator==(const Expr &x, const Expr &y) {
    return Rel(x.ex, Rel::eq, y.ex);
}

std::ostream& operator<<(std::ostream &s, const Expr &e) {
    s << e.ex;
    return s;
}


Expr Rel::lhs() const {
    return l;
}

Expr Rel::rhs() const {
    return r;
}

Rel Rel::expand() const {
    return Rel(l.expand(), op, r.expand());
}

bool Rel::isPoly() const {
    return l.isPoly() && r.isPoly();
}

bool Rel::isLinear(const option<VarSet> &vars) const {
    return l.isLinear(vars) && r.isLinear(vars);
}

bool Rel::isInequality() const {
    return op != Rel::eq && op != Rel::neq;
}

bool Rel::isGreaterThanZero() const {
    return op == Rel::gt && r.isZero();
}

Rel Rel::toLessEq() const {
    assert(isInequality());

    Rel res = *this;
    //flip > or >=
    if (res.op == Rel::gt) {
        res = res.rhs() < res.lhs();
    } else if (op == Rel::geq) {
        res = res.rhs() <= res.lhs();
    }

    //change < to <=, assuming integer arithmetic
    if (res.op == Rel::lt) {
        res = res.lhs() <= (res.rhs() - 1);
    }

    assert(res.op == Rel::leq);
    return res;
}

Rel Rel::toGreater() const {
    assert(isInequality());

    Rel res = *this;
    //flip < or <=
    if (res.op == Rel::lt) {
        res = res.rhs() > res.lhs();
    } else if (op == Rel::leq) {
        res = res.rhs() >= res.lhs();
    }

    //change >= to >, assuming integer arithmetic
    if (res.op == Rel::geq) {
        res = (res.lhs() + 1) > res.rhs();
    }

    assert(res.op == Rel::gt);
    return res;
}

Rel Rel::toLessOrLessEq() const {
    assert(isInequality());

    if (op == Rel::gt) {
        return r < l;
    } else if (op == Rel::geq) {
        return r <= l;
    }

    return *this;
}

Rel Rel::splitVariablesAndConstants(const VarSet &params) const {
    assert(isInequality());

    //move everything to lhs
    Expr newLhs = l - r;
    Expr newRhs = 0;

    //move all numerical constants back to rhs
    newLhs = newLhs.expand();
    if (newLhs.isAdd()) {
        for (size_t i=0; i < newLhs.arity(); ++i) {
            bool isConstant = true;
            VarSet vars = newLhs.op(i).vars();
            for (const Var &var: vars) {
                if (params.find(var) == params.end()) {
                    isConstant = false;
                    break;
                }
            }
            if (isConstant) {
                newRhs = newRhs - newLhs.op(i);
            }
        }
    } else {
        VarSet vars = newLhs.vars();
        bool isConstant = true;
        for (const Var &var: vars) {
            if (params.find(var) == params.end()) {
                isConstant = false;
                break;
            }
        }
        if (isConstant) {
            newRhs = newRhs - newLhs;
        }
    }
    //other cases (mul, pow, sym) should not include numerical constants (only numerical coefficients)

    newLhs = newLhs + newRhs;
    return Rel(newLhs, op, newRhs);
}

bool Rel::isTriviallyTrue() const {
    auto optTrivial = checkTrivial();
    return (optTrivial && optTrivial.get());
}

bool Rel::isTriviallyFalse() const {
    auto optTrivial = checkTrivial();
    return (optTrivial && !optTrivial.get());
}

option<bool> Rel::checkTrivial() const {
    Expr diff = (l - r).expand();
    if (diff.isRationalConstant()) {
        switch (op) {
        case Rel::eq: return diff.isZero();
        case Rel::neq: return !diff.isZero();
        case Rel::lt: return diff.toNum().is_negative();
        case Rel::leq: return !diff.toNum().is_positive();
        case Rel::gt: return diff.toNum().is_positive();
        case Rel::geq: return !diff.toNum().is_negative();
        }
        assert(false && "unknown relation");
    }
    return {};
}

void Rel::collectVariables(VarSet &res) const {
    l.collectVars(res);
    r.collectVars(res);
}

bool Rel::has(const Expr &pattern) const {
    return l.has(pattern) || r.has(pattern);
}

Rel Rel::subs(const ExprMap &map) const {
    return Rel(l.subs(map), op, r.subs(map));
}

Rel Rel::replace(const ExprMap &patternMap) const {
    return Rel(l.subs(patternMap), op, r.subs(patternMap));
}

void Rel::applySubs(const ExprMap &subs) {
    l.applySubs(subs);
    r.applySubs(subs);
}

std::string Rel::toString() const {
    stringstream s;
    s << this;
    return s.str();
}

Rel::Operator Rel::getOp() const {
    return op;
}

VarSet Rel::getVariables() const {
    VarSet res;
    collectVariables(res);
    return res;
}

int Rel::compare(const Rel &that) const {
    int fst = l.compare(that.l);
    if (fst != 0) {
        return fst < 0;
    }
    return r.compare(that.r);
}

Rel Rel::fromString(const string &s, const GiNaC::lst &variables) {
    auto containsRelations = [](const string &s) -> bool {
        return s.find_first_of("<>=") != string::npos;
    };

    assert(containsRelations(s));

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

            Expr lhsExpr = GiNaC::ex(lhs,variables);
            Expr rhsExpr = GiNaC::ex(rhs,variables);

            if (op == "<") return lhsExpr < rhsExpr;
            else if (op == ">") return lhsExpr > rhsExpr;
            else if (op == "<=") return lhsExpr <= rhsExpr;
            else if (op == ">=") return lhsExpr >= rhsExpr;
            else if (op == "!=") return lhsExpr != rhsExpr;
            else return lhsExpr == rhsExpr;
        }
    }
    assert(false && "unreachable");
}

Rel Rel::normalizeInequality() const {
    assert(isInequality());
    Rel greater = toGreater();
    Rel normalized = (greater.lhs() - greater.rhs()) > 0;
    assert(normalized.isGreaterThanZero());
    return normalized;
}

Rel operator!(const Rel &x) {
    switch (x.op) {
    case Rel::eq: return Rel(x.l, Rel::neq, x.r);
    case Rel::neq: return Rel(x.l, Rel::eq, x.r);
    case Rel::lt: return Rel(x.l, Rel::geq, x.r);
    case Rel::leq: return Rel(x.l, Rel::gt, x.r);
    case Rel::gt: return Rel(x.l, Rel::leq, x.r);
    case Rel::geq: return Rel(x.l, Rel::lt, x.r);
    }
    assert(false && "unknown relation");
}

bool operator==(const Rel &x, const Rel &y) {
    return x.l.equals(y.l) && x.op == y.op && x.r.equals(y.r);
}

bool operator!=(const Rel &x, const Rel &y) {
    return !(x == y);
}

bool operator<(const Rel &x, const Rel &y) {
    int fst = x.lhs().compare(y.lhs());
    if (fst != 0) {
        return fst < 0;
    }
    if (x.getOp() != y.getOp()) {
        return x.getOp() < y.getOp();
    }
    return x.rhs().compare(y.rhs()) < 0;
}

Rel operator<(const Var &x, const Expr &y) {
    return Expr(x) < y;
}

Rel operator<(const Expr &x, const Var &y) {
    return x < Expr(y);
}

Rel operator<(const Var &x, const Var &y) {
    return Expr(x) < Expr(y);
}

Rel operator>(const Var &x, const Expr &y) {
    return Expr(x) > y;
}

Rel operator>(const Expr &x, const Var &y) {
    return x > Expr(y);
}

Rel operator>(const Var &x, const Var &y) {
    return Expr(x) > Expr(y);
}

Rel operator<=(const Var &x, const Expr &y) {
    return Expr(x) <= y;
}

Rel operator<=(const Expr &x, const Var &y) {
    return x <= Expr(y);
}

Rel operator<=(const Var &x, const Var &y) {
    return Expr(x) <= Expr(y);
}

Rel operator>=(const Var &x, const Expr &y) {
    return Expr(x) >= y;
}

Rel operator>=(const Expr &x, const Var &y) {
    return x >= Expr(y);
}

Rel operator>=(const Var &x, const Var &y) {
    return Expr(x) >= Expr(y);
}

std::ostream& operator<<(std::ostream &s, const Rel &rel) {
    s << rel.l;
    switch (rel.op) {
    case Rel::eq: s << " == ";
        break;
    case Rel::neq: s << " 1= ";
        break;
    case Rel::leq: s << " <= ";
        break;
    case Rel::geq: s << " >= ";
        break;
    case Rel::lt: s << " < ";
        break;
    case Rel::gt: s << " > ";
        break;
    }
    s << rel.r;
    return s;
}


ExprMap::ExprMap() { }

ExprMap::ExprMap(const Expr &key, const Expr &val) {
    put(key, val);
}

Expr ExprMap::get(const Expr &key) const {
    return map.at(key);
}

void ExprMap::put(const Expr &key, const Expr &val) {
    map[key] = val;
    ginacMap[key.ex] = val.ex;
}

std::map<Expr, Expr, Expr_is_less>::const_iterator ExprMap::begin() const {
    return map.begin();
}

std::map<Expr, Expr, Expr_is_less>::const_iterator ExprMap::end() const {
    return map.end();
}

std::map<Expr, Expr, Expr_is_less>::const_iterator ExprMap::find(const Expr &e) const {
    return map.find(e);
}

bool ExprMap::contains(const Expr &e) const {
    return map.count(e) > 0;
}

bool ExprMap::empty() const {
    return map.empty();
}

ExprMap ExprMap::compose(const ExprMap &that) const {
    ExprMap res;
    for (const auto &p: *this) {
        res.put(p.first, p.second.subs(that));
    }
    for (const auto &p: that) {
        if (!res.contains(p.first)) {
            res.put(p.first, p.second);
        }
    }
    return res;
}

bool operator<(const ExprMap &x, const ExprMap &y) {
    auto it1 = x.begin();
    auto it2 = y.begin();
    while (it1 != x.end() && it2 != y.end()) {
        int fst = it1->first.compare(it2->first);
        if (fst != 0) {
            return fst < 0;
        }
        int snd = it1->second.compare(it2->second);
        if (snd != 0) {
            return snd < 0;
        }
        ++it1;
        ++it2;
    }
    return it1 == x.end() && it2 != y.end();
}
