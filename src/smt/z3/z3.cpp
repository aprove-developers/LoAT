#include "z3.hpp"
#include "../ginactosmt.hpp"

std::ostream& Z3::print(std::ostream& os) const {
    return os << solver;
}

Z3::~Z3() {}

Z3::Z3(const VariableManager &varMan): varMan(varMan), solver(ctx) {
    updateParams();
}

bool Z3::add(const BoolExpr &e) {
    solver.add(GinacToSmt<z3::expr>::convert(e, *this, varMan));
    return true;
}

void Z3::push() {
    solver.push();
}

void Z3::pop() {
    solver.pop();
}

smt::Result Z3::check() {
    switch (solver.check()) {
    case z3::sat: return smt::Sat;
    case z3::unsat: return smt::Unsat;
    case z3::unknown: return smt::Unknown;
    }
    assert(false && "unknown result");
}

VarMap<GiNaC::numeric> Z3::model() {
    assert(models);
    const z3::model &m = solver.get_model();
    VarMap<GiNaC::numeric> res;
    for (const auto &p: symbolMap) {
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
    z3::params params(ctx);
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

z3::expr Z3::var(const std::string &name, Expr::Type type) {
    return (type == Expr::Int) ? ctx.int_const(name.c_str()) : ctx.real_const(name.c_str());
}

z3::expr Z3::getInt(long val) {
    return ctx.int_val(val);
}

z3::expr Z3::getReal(long num, long denom) {
    return ctx.real_val(num, denom);
}

z3::expr Z3::pow(const z3::expr &base, const z3::expr &exp) {
    return z3::pw(base, exp);
}

z3::expr Z3::plus(const z3::expr &x, const z3::expr &y) {
    return x + y;
}

z3::expr Z3::times(const z3::expr &x, const z3::expr &y) {
    return x * y;
}

z3::expr Z3::eq(const z3::expr &x, const z3::expr &y) {
    return x == y;
}

z3::expr Z3::lt(const z3::expr &x, const z3::expr &y) {
    return x < y;
}

z3::expr Z3::le(const z3::expr &x, const z3::expr &y) {
    return x <= y;
}

z3::expr Z3::gt(const z3::expr &x, const z3::expr &y) {
    return x > y;
}

z3::expr Z3::ge(const z3::expr &x, const z3::expr &y) {
    return x >= y;
}

z3::expr Z3::neq(const z3::expr &x, const z3::expr &y) {
    return x != y;
}

z3::expr Z3::bAnd(const z3::expr &x, const z3::expr &y) {
    return x && y;
}

z3::expr Z3::bOr(const z3::expr &x, const z3::expr &y) {
    return x || y;
}

z3::expr Z3::bTrue() {
    return ctx.bool_val(true);
}

z3::expr Z3::bFalse() {
    return ctx.bool_val(false);
}
