#ifdef HAS_CVC4

#include "cvc4context.hpp"

Cvc4Context::Cvc4Context(CVC4::ExprManager &manager): manager(manager) { }

Cvc4Context::~Cvc4Context() { }

CVC4::Expr Cvc4Context::buildVar(const std::string &name, Expr::Type type) {
    return (type == Expr::Int) ? manager.mkVar(name, manager.integerType()) : manager.mkVar(name, manager.realType());
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

CVC4::Expr Cvc4Context::bTrue() {
    return manager.mkConst(true);
}

CVC4::Expr Cvc4Context::bFalse() {
    return manager.mkConst(false);
}

#endif
