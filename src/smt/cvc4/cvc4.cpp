#ifdef HAS_CVC4

#include "cvc4.hpp"
#include "../../config.hpp"
#include "../ginactosmt.hpp"

Cvc4::Cvc4(const VariableManager &varMan): varMan(varMan), solver(&manager) { }

bool Cvc4::add(const BoolExpr &e) {
    solver.assertFormula(GinacToSmt<CVC4::Expr>::convert(e, *this, varMan));
    return true;
}

void Cvc4::push() {
    solver.push();
}

void Cvc4::pop() {
    solver.pop();
}

smt::Result Cvc4::check() {
    switch (solver.checkSat().isSat()) {
    case CVC4::Result::SAT: return smt::Sat;
    case CVC4::Result::UNSAT: return smt::Unsat;
    case CVC4::Result::SAT_UNKNOWN: return smt::Unknown;
    }
    assert(false && "unknown result");
}

GiNaC::numeric Cvc4::getRealFromModel(const CVC4::Expr &symbol) {
    CVC4::Rational rat = solver.getValue(symbol).getConst<CVC4::Rational>();
    GiNaC::numeric res = rat.getNumerator().getLong();
    return res / rat.getDenominator().getLong();
}

VarMap<GiNaC::numeric> Cvc4::model() {
    assert(models);
    VarMap<GiNaC::numeric> res;
    for (const auto &p: symbolMap) {
        res[p.first] = getRealFromModel(p.second);
    }
    return res;
}

void Cvc4::setTimeout(unsigned int timeout) {
    this->timeout = timeout;
    solver.setTimeLimit(timeout);
}

void Cvc4::enableModels() {
    this->models = true;
    solver.setOption("produce-models", true);
}

void Cvc4::resetSolver() {
    solver.reset();
    solver.setTimeLimit(timeout);
}

CVC4::Expr Cvc4::var(const std::string &name, Expr::Type type) {
    return (type == Expr::Int) ? manager.mkVar(name, manager.integerType()) : manager.mkVar(name, manager.realType());
}

CVC4::Expr Cvc4::getInt(long val) {
    return manager.mkConst(CVC4::Rational(val, 1l));
}

CVC4::Expr Cvc4::getReal(long num, long denom) {
    return manager.mkConst(CVC4::Rational(num, denom));
}

CVC4::Expr Cvc4::pow(const CVC4::Expr &base, const CVC4::Expr &exp) {
    return manager.mkExpr(CVC4::Kind::POW, base, exp);
}

CVC4::Expr Cvc4::plus(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::PLUS, x, y);
}

CVC4::Expr Cvc4::times(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::MULT, x, y);
}

CVC4::Expr Cvc4::eq(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::EQUAL, x, y);
}

CVC4::Expr Cvc4::lt(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::LT, x, y);
}

CVC4::Expr Cvc4::le(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::LEQ, x, y);
}

CVC4::Expr Cvc4::gt(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::GT, x, y);
}

CVC4::Expr Cvc4::ge(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::GEQ, x, y);
}

CVC4::Expr Cvc4::neq(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::NOT, manager.mkExpr(CVC4::Kind::EQUAL, x, y));
}

CVC4::Expr Cvc4::bAnd(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::AND, x, y);
}

CVC4::Expr Cvc4::bOr(const CVC4::Expr &x, const CVC4::Expr &y) {
    return manager.mkExpr(CVC4::Kind::OR, x, y);
}

CVC4::Expr Cvc4::bTrue() {
    return manager.mkConst(true);
}

CVC4::Expr Cvc4::bFalse() {
    return manager.mkConst(false);
}

Cvc4::~Cvc4() {}

#endif
