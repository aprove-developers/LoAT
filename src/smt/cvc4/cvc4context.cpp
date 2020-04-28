#include "cvc4context.hpp"

Cvc4Context::Cvc4Context(CVC4::ExprManager &manager): manager(manager) { }

Cvc4Context::~Cvc4Context() { }

CVC4::Expr Cvc4Context::buildVar(const std::string &name, Expr::Type type) {
    CVC4::Expr res = (type == Expr::Int) ? manager.mkVar(name, manager.integerType()) : manager.mkVar(name, manager.realType());
    varNames[res] = name;
    return res;
}

CVC4::Expr Cvc4Context::buildBoundVar(const std::string &name, Expr::Type type) {
    CVC4::Expr res = (type == Expr::Int) ? manager.mkBoundVar(name, manager.integerType()) : manager.mkBoundVar(name, manager.realType());
    varNames[res] = name;
    return res;
}

CVC4::Expr Cvc4Context::buildConst(uint id) {
    return manager.mkVar(manager.booleanType());
}

CVC4::Expr Cvc4Context::getInt(long val) {
    return manager.mkConst(CVC4::Rational(val, 1l));
}

CVC4::Expr Cvc4Context::getReal(long num, long denom) {
    return manager.mkConst(CVC4::Rational(num, denom));
}

CVC4::Expr Cvc4Context::pow(const CVC4::Expr &base, const CVC4::Expr &exp) {
    return manager.mkExpr(CVC4::Kind::POW, base, exp);
}

CVC4::Expr Cvc4Context::plus(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::PLUS, x, y);
}

CVC4::Expr Cvc4Context::times(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::MULT, x, y);
}

CVC4::Expr Cvc4Context::eq(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::EQUAL, x, y);
}

CVC4::Expr Cvc4Context::lt(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::LT, x, y);
}

CVC4::Expr Cvc4Context::le(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::LEQ, x, y);
}

CVC4::Expr Cvc4Context::gt(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::GT, x, y);
}

CVC4::Expr Cvc4Context::ge(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::GEQ, x, y);
}

CVC4::Expr Cvc4Context::neq(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::NOT, manager.mkExpr(CVC4::Kind::EQUAL, x, y));
}

CVC4::Expr Cvc4Context::bAnd(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::AND, x, y);
}

CVC4::Expr Cvc4Context::bOr(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::OR, x, y);
}

CVC4::Expr Cvc4Context::bTrue() const {
    return manager.mkConst(true);
}

CVC4::Expr Cvc4Context::bFalse() const {
    return manager.mkConst(false);
}

CVC4::Expr Cvc4Context::negate(const CVC4::Expr &x) {
    return x.notExpr();
}

CVC4::Expr Cvc4Context::forall(const std::vector<CVC4::Expr> &vars, const CVC4::Expr &body) {
    const CVC4::Expr &boundVarList = manager.mkExpr(CVC4::Kind::BOUND_VAR_LIST, vars);
    return manager.mkExpr(CVC4::Kind::FORALL, boundVarList, body);
}

bool Cvc4Context::isLit(const CVC4::Expr &e) const {
    switch (e.getKind()) {
    case CVC4::Kind::GT:
    case CVC4::Kind::LT:
    case CVC4::Kind::GEQ:
    case CVC4::Kind::LEQ:
    case CVC4::Kind::DISTINCT:
    case CVC4::Kind::EQUAL: return true;
    default: return false;
    }
}

bool Cvc4Context::isTrue(const CVC4::Expr &e) const {
    return e.isConst() && e.getType().isBoolean() && e.getConst<bool>();
}

bool Cvc4Context::isFalse(const CVC4::Expr &e) const {
    return e.isConst() && e.getType().isBoolean() && !e.getConst<bool>();
}

bool Cvc4Context::isNot(const CVC4::Expr &e) const {
    return e.getKind() == CVC4::Kind::NOT;
}

std::vector<CVC4::Expr> Cvc4Context::getChildren(const CVC4::Expr &e) const {
    return e.getChildren();
}

bool Cvc4Context::isAnd(const CVC4::Expr &e) const {
    return e.getKind() == CVC4::Kind::AND;
}

bool Cvc4Context::isAdd(const CVC4::Expr &e) const {
    return e.getKind() == CVC4::Kind::PLUS;
}

bool Cvc4Context::isMul(const CVC4::Expr &e) const {
    return e.getKind() == CVC4::Kind::MULT;
}

bool Cvc4Context::isDiv(const CVC4::Expr &e) const {
    return e.getKind() == CVC4::Kind::DIVISION;
}

bool Cvc4Context::isPow(const CVC4::Expr &e) const {
    return e.getKind() == CVC4::Kind::POW;
}

bool Cvc4Context::isVar(const CVC4::Expr &e) const {
    return e.isVariable();
}

bool Cvc4Context::isRationalConstant(const CVC4::Expr &e) const {
    return (e.getType().isReal() || e.getType().isInteger()) && e.isConst();
}

bool Cvc4Context::isInt(const CVC4::Expr &e) const {
    return e.getType().isInteger();
}

long Cvc4Context::toInt(const CVC4::Expr &e) const {
    assert(isInt(e));
    return e.getConst<CVC4::Rational>().getNumerator().getLong();
}

long Cvc4Context::numerator(const CVC4::Expr &e) const {
    return e.getConst<CVC4::Rational>().getNumerator().getLong();
}

long Cvc4Context::denominator(const CVC4::Expr &e) const {
    return e.getConst<CVC4::Rational>().getDenominator().getLong();
}

CVC4::Expr Cvc4Context::lhs(const CVC4::Expr &e) const {
    assert(e.getNumChildren() == 2);
    return e.getChildren()[0];
}

CVC4::Expr Cvc4Context::rhs(const CVC4::Expr &e) const {
    assert(e.getNumChildren() == 2);
    return e.getChildren()[1];
}

Rel::RelOp Cvc4Context::relOp(const CVC4::Expr &e) const {
    switch (e.getKind()) {
    case CVC4::Kind::GT: return Rel::gt;
    case CVC4::Kind::LT: return Rel::lt;
    case CVC4::Kind::GEQ: return Rel::geq;
    case CVC4::Kind::LEQ: return Rel::leq;
    case CVC4::Kind::DISTINCT:
        assert(e.getNumChildren() == 2);
        return Rel::neq;
    case CVC4::Kind::EQUAL: return Rel::eq;
    default: assert(false && "unknown relation");
    }
}

std::string Cvc4Context::getName(const CVC4::Expr &e) const {
    return varNames.at(e);
}

void Cvc4Context::printStderr(const CVC4::Expr &e) const {
    std::cerr << e << std::endl;
}
