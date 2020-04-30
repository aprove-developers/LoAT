#include "z3.hpp"
#include "../exprtosmt.hpp"
#include "../smttoexpr.hpp"

std::ostream& Z3::print(std::ostream& os) const {
    return os << solver;
}

Z3::~Z3() {}

Z3::Z3(VariableManager &varMan): Smt(varMan), ctx(z3Ctx), solver(z3Ctx) {
    updateParams();
}

void Z3::_add(const BoolExpr e) {
    solver.add(ExprToSmt<z3::expr>::convert(e, ctx, varMan));
}

void Z3::_push() {
    solver.push();
}

void Z3::_pop() {
    solver.pop();
}

Smt::Result Z3::check() {
    z3::check_result res;
    if (unsatCores) {
        z3Marker = z3::expr_vector(z3Ctx);
        for (const BoolExpr &m: marker) {
            z3Marker->push_back(ExprToSmt<z3::expr>::convert(m, ctx, varMan));
        }
        res = solver.check(z3Marker.get());
    } else {
        res = solver.check();
    }
    switch (res) {
    case z3::sat: return Sat;
    case z3::unsat: return Unsat;
    case z3::unknown: return Unknown;
    }
    assert(false && "unknown result");
}

Model Z3::model() {
    assert(models);
    const z3::model &m = solver.get_model();
    VarMap<GiNaC::numeric> vars;
    for (const auto &p: ctx.getSymbolMap()) {
        vars[p.first] = getRealFromModel(m, p.second);
    }
    std::map<uint, bool> constants;
    for (const auto &p: ctx.getConstMap()) {
        constants[p.first] = m.eval(p.second).bool_value();
    }
    return Model(vars, constants);
}

void Z3::updateParams() {
    z3::params params(z3Ctx);
    params.set(":model", models);
    params.set(":timeout", timeout);
    params.set(":unsat-core", unsatCores);
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

std::vector<uint> Z3::unsatCore() {
    const z3::expr_vector &core = solver.unsat_core();
    std::vector<uint> res;
    for (const z3::expr &e: core) {
        bool found = false;
        for (uint i = 0; i < z3Marker->size(); ++i) {
            // both are variables, so comparing their string representation is (ugly but) fine
            if (e.to_string() == z3Marker.get()[i].to_string()) {
                res.push_back(i);
                found = true;
                break;
            }
        }
        assert(found);
    }
    return res;
}

void Z3::_resetSolver() {
    solver.reset();
}

void Z3::_resetContext() {
    ctx.reset();
}

BoolExpr Z3::simplify(const BoolExpr expr, const VariableManager &varMan) {
    z3::context z3Ctx;
    Z3Context ctx(z3Ctx);
    z3::tactic t(z3Ctx, "ctx-solver-simplify");
    z3::solver s = t.mk_solver();
    const z3::expr &converted = ExprToSmt<z3::expr>::convert(expr, ctx, varMan);
    s.add(converted);
    s.check();
    std::vector<BoolExpr> simplified;
    for (const z3::expr &e: s.assertions()) {
        simplified.push_back(SmtToExpr<z3::expr>::convert(e, ctx));
    }
    return buildAnd(simplified);
}
