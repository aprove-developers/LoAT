#include "cvc4context.hpp"

Cvc4Context::~Cvc4Context() { }

CVC4::Expr Cvc4Context::buildVar(const std::string &name, Expression::Type type) {
    return (type == Expression::Int) ? mkVar(name, integerType()) : mkVar(name, realType());
}

CVC4::Expr Cvc4Context::getInt(long val) {
    return mkConst(CVC4::Rational(val));
}

CVC4::Expr Cvc4Context::getReal(long num, long denom) {
    return mkConst(CVC4::Rational(num, denom));
}

CVC4::Expr Cvc4Context::pow(const CVC4::Expr &base, const CVC4::Expr &exp) {
    return mkExpr(CVC4::Kind::POW, base, exp);
}

CVC4::Expr Cvc4Context::plus(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::PLUS, x, y);
}

CVC4::Expr Cvc4Context::times(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::MULT, x, y);
}

CVC4::Expr Cvc4Context::eq(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::EQUAL, x, y);
}

CVC4::Expr Cvc4Context::lt(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::LT, x, y);
}

CVC4::Expr Cvc4Context::le(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::LEQ, x, y);
}

CVC4::Expr Cvc4Context::gt(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::GT, x, y);
}

CVC4::Expr Cvc4Context::ge(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::GEQ, x, y);
}

CVC4::Expr Cvc4Context::neq(const CVC4::Expr &x, const CVC4::Expr &y) {
    return mkExpr(CVC4::Kind::NOT, mkExpr(CVC4::Kind::EQUAL, x, y));
}
