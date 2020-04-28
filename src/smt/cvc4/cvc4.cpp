#include "cvc4.hpp"
#include "../../config.hpp"
#include "../exprtosmt.hpp"

Cvc4::Cvc4(const VariableManager &varMan): varMan(varMan), ctx(manager), solver(&manager) { }

uint Cvc4::add(const BoolExpr &e) {
    solver.assertFormula(ExprToSmt<CVC4::Expr>::convert(e, ctx, varMan));
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
    assert(false && "unknown result");
}

GiNaC::numeric Cvc4::getRealFromModel(const CVC4::Expr &symbol) {
    CVC4::Rational rat = solver.getValue(symbol).getConst<CVC4::Rational>();
    GiNaC::numeric res = rat.getNumerator().getLong();
    return res / rat.getDenominator().getLong();
}

Model Cvc4::model() {
    assert(models);
    VarMap<GiNaC::numeric> vars;
    for (const auto &p: ctx.getSymbolMap()) {
        vars[p.first] = getRealFromModel(p.second);
    }
    std::map<uint, bool> constants;
    for (const auto &p: ctx.getConstMap()) {
        constants[p.first] = solver.getValue(p.second).getConst<bool>();
    }
    return Model(vars, constants);
}

Subs Cvc4::modelSubs() {
    assert(models);
    Subs res;
    for (const auto &p: ctx.getSymbolMap()) {
        res.put(p.first, getRealFromModel(p.second));
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

void Cvc4::enableUnsatCores() {
    this->unsatCores = true;
    solver.setOption("produce-unsat-cores", true);
}

void Cvc4::resetSolver() {
    solver.reset();
    solver.setTimeLimit(timeout);
    solver.setOption("produce-models", models);
    solver.setOption("produce-unsat-cores", unsatCores);
}


Cvc4::~Cvc4() {}
