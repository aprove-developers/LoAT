#include "z3.hpp"
#include "ginactoz3.hpp"
#include "../../config.hpp"

std::ostream& Z3::print(std::ostream& os) const {
    return os << solver;
}

Z3::~Z3() {}

Z3::Z3(const VariableManager &varMan): varMan(varMan), solver(z3::solver(ctx)) {
    setTimeout(Config::Z3::DefaultTimeout);
}

void Z3::add(const BoolExpr &e) {
    solver.add(convert(e));
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
}

ExprSymbolMap<GiNaC::numeric> Z3::model() {
    const z3::model &m = solver.get_model();
    ExprSymbolMap<GiNaC::numeric> res;
    for (const auto &p: ctx.getSymbolMap()) {
        res[p.first] = getRealFromModel(m, p.second);
    }
    return res;
}

void Z3::setTimeout(unsigned int timeout) {
    if (this->timeout == timeout) {
        return;
    } else if (timeout > 0) {
        z3::params params(ctx);
        params.set(":timeout", timeout);
        solver.set(params);
        this->timeout = timeout;
    }
}

z3::expr Z3::convert(const BoolExpr &e) {
    if (e->getLit()) {
        return GinacToZ3::convert(e->getLit().get(), ctx, varMan);
    }
    z3::expr res = ctx.bool_val(e->isAnd());
    bool first = true;
    for (const BoolExpr &c: e->getChildren()) {
        if (first) {
            res = convert(c);
            first = false;
        } else {
            res = e->isAnd() ? (res && convert(c)) : (res || convert(c));
        }
    }
    return res;
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
}
