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

bool Expression_is_less::operator()(const Expression &lh, const Expression &rh) const {
     return lh.compare(rh) < 0;
}

bool Relation_is_less::operator()(const Rel &lh, const Rel &rh) const {
    int fst = lh.lhs().compare(rh.lhs());
    if (fst != 0) {
        return fst < 0;
    }
    return lh.rhs().compare(rh.rhs()) < 0;
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

const ExprSymbol Expression::NontermSymbol = GiNaC::symbol("NONTERM");

Expression Expression::fromString(const string &s, const GiNaC::lst &variables) {
    auto containsRelations = [](const string &s) -> bool {
        return s.find_first_of("<>=") != string::npos;
    };
    assert(!containsRelations(s));
    return Expression(GiNaC::ex(s,variables));
}

void Expression::applySubs(const ExprMap &subs) {
    this->ex = this->ex.subs(subs.ginacMap);
}

bool Expression::findAll(const Expression &pattern, ExpressionSet &found) const {
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


bool Expression::isLinear(const option<ExprSymbolSet> &vars) const {
    ExprSymbolSet theVars = vars ? vars.get() : getVariables();
    // linear expressions are always polynomials
    if (!isPolynomial()) return false;

    // degree only works reliable on expanded expressions (despite the tutorial stating otherwise)
    Expression expanded = expand();

    // GiNaC does not provide an info flag for this, so we check the degree of every variable.
    // We also have to check if the coefficient contains variables,
    // e.g. y has degree 1 in x*y, but we don't consider x*y to be linear.
    for (const ExprSymbol &var : theVars) {
        int deg = expanded.degree(var);
        if (deg > 1 || deg < 0) {
            return false;
        }

        if (deg == 1) {
            ExprSymbolSet coefficientVars = Expression(expanded.coeff(var,deg)).getVariables();
            for (const ExprSymbol &e: coefficientVars) {
                if (theVars.find(e) != theVars.end()) {
                    return false;
                }
            }
        }
    }
    return true;
}


bool Expression::isPolynomial() const {
    return ex.info(GiNaC::info_flags::polynomial);
}

bool Expression::isPolynomial(const ExprSymbol &n) const {
    return ex.is_polynomial(n);
}


bool Expression::isPolynomialWithIntegerCoeffs() const {
    return ex.info(GiNaC::info_flags::integer_polynomial);
}


bool Expression::isIntegerConstant() const {
    return ex.info(GiNaC::info_flags::integer);
}


bool Expression::isRationalConstant() const {
    return ex.info(GiNaC::info_flags::rational);
}


bool Expression::isProperRational() const {
    return ex.info(GiNaC::info_flags::rational)
           && !ex.info(GiNaC::info_flags::integer);
}


bool Expression::isProperNaturalPower() const {
    if (!this->isPower()) {
        return false;
    }

    Expression power = this->op(1);
    if (!power.isIntegerConstant()) {
        return false;
    }

    return power.toNumeric() > 1;
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


Complexity Expression::getComplexity(const Expression &term) {

    //traverse the expression
    if (term.isNumeric()) {
        GiNaC::numeric num = term.toNumeric();
        assert(num.is_integer() || num.is_real());
        //both for positive and negative constants, as we want to over-approximate the complexity! (e.g. A-B is O(n))
        return Complexity::Const;

    } else if (term.isPower()) {
        assert(term.nops() == 2);

        // If the exponent is at least polynomial (non-constant), complexity might be exponential
        if (getComplexity(term.op(1)) > Complexity::Const) {
            const Expression &base = term.op(0);
            if (base.is_zero() || base.compare(1) == 0 || base.compare(-1) == 0) {
                return Complexity::Const;
            }
            return Complexity::Exp;

        // Otherwise the complexity is polynomial, if the exponent is nonnegative
        } else {
            if (!term.op(1).isNumeric()) {
                return Complexity::Unknown;
            }
            GiNaC::numeric numexp = term.op(1).toNumeric();
            if (!numexp.is_nonneg_integer()) {
                return Complexity::Unknown;
            }

            Complexity base = getComplexity(term.op(0));
            int exp = numexp.to_int();
            return base ^ exp;
        }

    } else if (term.isMul()) {
        assert(term.nops() > 0);
        Complexity cpx = getComplexity(term.op(0));
        for (unsigned int i=1; i < term.nops(); ++i) {
            cpx = cpx * getComplexity(term.op(i));
        }
        return cpx;

    } else if (term.isAdd()) {
        assert(term.nops() > 0);
        Complexity cpx = getComplexity(term.op(0));
        for (unsigned int i=1; i < term.nops(); ++i) {
            cpx = cpx + getComplexity(term.op(i));
        }
        return cpx;

    } else if (term.isSymbol()) {
        return (term.compare(Expression::NontermSymbol) == 0) ? Complexity::Nonterm : Complexity::Poly(1);
    }

    //unknown expression type (e.g. relational)
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

bool Expression::is_equal(const Expression &that) const {
    return ex.is_equal(that.ex);
}

int Expression::degree(const ExprSymbol &var) const {
    return ex.degree(var);
}

int Expression::ldegree(const ExprSymbol &var) const {
    return ex.ldegree(var);
}

Expression Expression::coeff(const ExprSymbol &var, int degree) const {
    return ex.coeff(var, degree);
}

Expression Expression::lcoeff(const ExprSymbol &var) const {
    return ex.lcoeff(var);
}

Expression Expression::expand() const {
    return ex.expand();
}

bool Expression::has(const Expression &pattern) const {
    return ex.has(pattern.ex);
}

bool Expression::is_zero() const {
    return ex.is_zero();
}

bool Expression::isSymbol() const {
    return ex.info(GiNaC::info_flags::symbol);
}

bool Expression::isNumeric() const {
    return ex.info(GiNaC::info_flags::numeric);
}

bool Expression::isPower() const {
    return GiNaC::is_a<GiNaC::power>(ex);
}

bool Expression::isMul() const {
    return GiNaC::is_a<GiNaC::mul>(ex);
};

bool Expression::isAdd() const {
    return GiNaC::is_a<GiNaC::add>(ex);
}

ExprSymbol Expression::toSymbol() const {
    return GiNaC::ex_to<GiNaC::symbol>(ex);
}

GiNaC::numeric Expression::toNumeric() const {
    return GiNaC::ex_to<GiNaC::numeric>(ex);
}

Expression Expression::op(uint i) const {
    return ex.op(i);
}

size_t Expression::nops() const {
    return ex.nops();
}

Expression Expression::subs(const ExprMap &map, uint options) const {
    return ex.subs(map.ginacMap, options);
}

void Expression::traverse(GiNaC::visitor &v) const {
    ex.traverse(v);
}

int Expression::compare(const Expression &that) const {
    return ex.compare(that.ex);
}

Expression Expression::numer() const {
    return ex.numer();
}

Expression Expression::denom() const {
    return ex.denom();
}

bool Expression::match(const Expression &pattern) const {
    return ex.match(pattern.ex);
}

Expression operator-(const Expression &x) {
    return -x.ex;
}

Expression operator-(const Expression &x, const Expression &y) {
    return x.ex - y.ex;
}

Expression operator+(const Expression &x, const Expression &y) {
    return x.ex + y.ex;
}

Expression operator*(const Expression &x, const Expression &y) {
    return x.ex * y.ex;
}

Expression operator/(const Expression &x, const Expression &y) {
    return x.ex / y.ex;
}

Expression operator^(const Expression &x, const Expression &y) {
    return GiNaC::pow(x.ex, y.ex);
}


Rel::Rel(const Expression &lhs, Operator op, const Expression &rhs): l(lhs), r(rhs), op(op) { }

Rel operator<(const Expression &x, const Expression &y) {
    return Rel(x.ex, Rel::lt, y.ex);
}

Rel operator>(const Expression &x, const Expression &y) {
    return Rel(x.ex, Rel::gt, y.ex);
}

Rel operator<=(const Expression &x, const Expression &y) {
    return Rel(x.ex, Rel::leq, y.ex);
}

Rel operator>=(const Expression &x, const Expression &y) {
    return Rel(x.ex, Rel::geq, y.ex);
}

Rel operator!=(const Expression &x, const Expression &y) {
    return Rel(x.ex, Rel::neq, y.ex);
}

Rel operator==(const Expression &x, const Expression &y) {
    return Rel(x.ex, Rel::eq, y.ex);
}

std::ostream& operator<<(std::ostream &s, const Expression &e) {
    s << e.ex;
    return s;
}


Expression Rel::lhs() const {
    return l;
}

Expression Rel::rhs() const {
    return r;
}

Rel Rel::expand() const {
    return Rel(l.expand(), op, r.expand());
}

bool Rel::isPolynomial() const {
    return l.isPolynomial() && r.isPolynomial();
}

bool Rel::isLinear(const option<ExprSymbolSet> &vars) const {
    return l.isLinear(vars) && r.isLinear(vars);
}

bool Rel::isInequality() const {
    return op != Rel::eq && op != Rel::neq;
}

bool Rel::isGreaterThanZero() const {
    return op == Rel::gt && r.is_zero();
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

    assert(res.op == Rel::geq);
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

Rel Rel::splitVariablesAndConstants(const ExprSymbolSet &params) const {
    assert(isInequality());

    //move everything to lhs
    Expression newLhs = l - r;
    Expression newRhs = 0;

    //move all numerical constants back to rhs
    newLhs = newLhs.expand();
    if (newLhs.isAdd()) {
        for (size_t i=0; i < newLhs.nops(); ++i) {
            bool isConstant = true;
            ExprSymbolSet vars = newLhs.op(i).getVariables();
            for (const ExprSymbol &var: vars) {
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
        ExprSymbolSet vars = newLhs.getVariables();
        bool isConstant = true;
        for (const ExprSymbol &var: vars) {
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
    Expression diff = (l - r).expand();
    if (diff.isRationalConstant()) {
        switch (op) {
        case Rel::eq: return diff.is_zero();
        case Rel::neq: return !diff.is_zero();
        case Rel::lt: return diff.toNumeric().is_negative();
        case Rel::leq: return !diff.toNumeric().is_positive();
        case Rel::gt: return diff.toNumeric().is_positive();
        case Rel::geq: return !diff.toNumeric().is_negative();
        }
        assert(false && "unknown relation");
    }
    return {};
}

void Rel::collectVariables(ExprSymbolSet &res) const {
    l.collectVariables(res);
    r.collectVariables(res);
}

bool Rel::has(const Expression &pattern) const {
    return l.has(pattern) || r.has(pattern);
}

Rel Rel::subs(const ExprMap &map, uint options) const {
    return Rel(l.subs(map, options), op, r.subs(map, options));
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

ExprSymbolSet Rel::getVariables() const {
    ExprSymbolSet res;
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

            Expression lhsExpr = GiNaC::ex(lhs,variables);
            Expression rhsExpr = GiNaC::ex(rhs,variables);

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
    case Rel::gt: return Rel(x.l, Rel::geq, x.r);
    case Rel::geq: return Rel(x.l, Rel::lt, x.r);
    }
    assert(false && "unknown relation");
}

bool operator==(const Rel &x, const Rel &y) {
    return x.l.is_equal(y.l) && x.op == y.op && x.r.is_equal(y.r);
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

Rel operator<(const ExprSymbol &x, const Expression &y) {
    return Expression(x) < y;
}

Rel operator<(const Expression &x, const ExprSymbol &y) {
    return x < Expression(y);
}

Rel operator<(const ExprSymbol &x, const ExprSymbol &y) {
    return Expression(x) < Expression(y);
}

Rel operator>(const ExprSymbol &x, const Expression &y) {
    return Expression(x) > y;
}

Rel operator>(const Expression &x, const ExprSymbol &y) {
    return x > Expression(y);
}

Rel operator>(const ExprSymbol &x, const ExprSymbol &y) {
    return Expression(x) > Expression(y);
}

Rel operator<=(const ExprSymbol &x, const Expression &y) {
    return Expression(x) <= y;
}

Rel operator<=(const Expression &x, const ExprSymbol &y) {
    return x <= Expression(y);
}

Rel operator<=(const ExprSymbol &x, const ExprSymbol &y) {
    return Expression(x) <= Expression(y);
}

Rel operator>=(const ExprSymbol &x, const Expression &y) {
    return Expression(x) >= y;
}

Rel operator>=(const Expression &x, const ExprSymbol &y) {
    return x >= Expression(y);
}

Rel operator>=(const ExprSymbol &x, const ExprSymbol &y) {
    return Expression(x) >= Expression(y);
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

ExprMap::ExprMap(const Expression &key, const Expression &val) {
    put(key, val);
}

Expression ExprMap::get(const Expression &key) const {
    return map.at(key);
}

void ExprMap::put(const Expression &key, const Expression &val) {
    map[key] = val;
    ginacMap[key.ex] = val.ex;
}

std::map<Expression, Expression, Expression_is_less>::const_iterator ExprMap::begin() const {
    return map.begin();
}

std::map<Expression, Expression, Expression_is_less>::const_iterator ExprMap::end() const {
    return map.end();
}

std::map<Expression, Expression, Expression_is_less>::const_iterator ExprMap::find(const Expression &e) const {
    return map.find(e);
}

bool ExprMap::contains(const Expression &e) const {
    return map.count(e) > 0;
}

bool ExprMap::empty() const {
    return map.empty();
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
    }
    return it1 == x.end() && it2 != y.end();
}
