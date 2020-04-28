#include "cvc4.hpp"
#include "../../config.hpp"
#include "../exprtosmt.hpp"
#include "../smttoexpr.hpp"

Cvc4::Cvc4(const VariableManager &varMan): varMan(varMan), ctx(manager), solver(&manager) { }

void Cvc4::_add(const ForAllExpr &e) {
    solver.assertFormula(ExprToSmt<CVC4::Expr>::convert(e, ctx, varMan));
}

void Cvc4::_push() {
    solver.push();
}

void Cvc4::_pop() {
    solver.pop();
}

Smt::Result Cvc4::check() {
    std::vector<CVC4::Expr> assumptions;
    for (const BoolExpr &m: marker) {
        assumptions.push_back(ExprToSmt<CVC4::Expr>::convert(m, ctx, varMan));
    }
    switch (solver.checkSat(assumptions).isSat()) {
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

std::vector<uint> Cvc4::unsatCore() {
    const CVC4::UnsatCore &core = solver.getUnsatCore();
    std::vector<uint> res;
    for (const CVC4::Expr &e: core) {
        res.push_back(markerMap[SmtToExpr<CVC4::Expr>::convert(e, ctx)]);
    }
    return res;
}

void Cvc4::updateParams() {
    solver.setOption("produce-unsat-cores", unsatCores);
    solver.setOption("produce-models", models);
    solver.setTimeLimit(timeout);
}

void Cvc4::_resetSolver() {
    solver.reset();
}


Cvc4::~Cvc4() {}
