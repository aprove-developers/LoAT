#ifdef HAS_CVC4

#include "cvc4.hpp"
#include "../../config.hpp"
#include "../ginactosmt.hpp"

Cvc4::Cvc4(const VariableManager &varMan): varMan(varMan), solver(&ctx) { }

void Cvc4::add(const BoolExpr &e) {
    solver.assertFormula(GinacToSmt<CVC4::Expr>::convert(e, ctx, varMan));
}

void Cvc4::push() {
    solver.push();
}

void Cvc4::pop() {
    solver.pop();
}

Smt::Result Cvc4::check() {
    switch (solver.checkSat().isSat()) {
    case CVC4::Result::SAT: return Smt::Sat;
    case CVC4::Result::UNSAT: return Smt::Unsat;
    case CVC4::Result::SAT_UNKNOWN: return Smt::Unknown;
    }
}

GiNaC::numeric Cvc4::getRealFromModel(const CVC4::Expr &symbol) {
    CVC4::Rational rat = solver.getValue(symbol).getConst<CVC4::Rational>();
    GiNaC::numeric res = rat.getNumerator().getLong();
    return res / rat.getDenominator().getLong();
}

ExprSymbolMap<GiNaC::numeric> Cvc4::model() {
    assert(models);
    ExprSymbolMap<GiNaC::numeric> res;
    for (const auto &p: ctx.getSymbolMap()) {
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


Cvc4::~Cvc4() {}

#endif
