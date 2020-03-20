#include "z3.hpp"
#include "../exprtosmt.hpp"

std::ostream& Z3::print(std::ostream& os) const {
    return os << solver;
}

Z3::~Z3() {}

Z3::Z3(const VariableManager &varMan): varMan(varMan), ctx(z3Ctx), solver(z3Ctx) {
    updateParams();
}

void Z3::add(const BoolExpr &e) {
    solver.add(ExprToSmt<z3::expr>::convert(e, ctx, varMan));
}

void Z3::push() {
    solver.push();
}

void Z3::pop() {
    solver.pop();
}

Smt::Result Z3::check() {
    switch (solver.check()) {
    case z3::sat: return Sat;
    case z3::unsat: return Unsat;
    case z3::unknown: return Unknown;
    }
    assert(false && "unknown result");
}

VarMap<GiNaC::numeric> Z3::model() {
    assert(models);
    const z3::model &m = solver.get_model();
    VarMap<GiNaC::numeric> res;
    for (const auto &p: ctx.getSymbolMap()) {
        res[p.first] = getRealFromModel(m, p.second);
    }
    return res;
}

void Z3::setTimeout(unsigned int timeout) {
    this->timeout = timeout;
    updateParams();
}

void Z3::enableModels() {
    this->models = true;
    updateParams();
}

void Z3::updateParams() {
    z3::params params(z3Ctx);
    params.set(":model", models);
    params.set(":timeout", timeout);
    solver.set(params);
}

GiNaC::numeric Z3::getRealFromModel(const z3::model &model, const z3::expr &symbol) {
    int num,denom;
    Z3_get_numeral_int(model.ctx(),Z3_get_numerator(model.ctx(),model.eval(symbol,true)),&num);
    Z3_get_numeral_int(model.ctx(),Z3_get_denominator(model.ctx(),model.eval(symbol,true)),&denom);
    assert(denom != 0);

    GiNaC::numeric res = num;
    res = res / denom;
    return res;
}

void Z3::resetSolver() {
    solver.reset();
    updateParams();
}
