#include "z3.hpp"
#include "../exprtosmt.hpp"
#include "../smttoexpr.hpp"

std::ostream& Z3::print(std::ostream& os) const {
    return os << solver;
}

Z3::~Z3() {}

Z3::Z3(const VariableManager &varMan): varMan(varMan), ctx(z3Ctx), solver(z3Ctx), marker(z3Ctx) {
    updateParams();
}

uint Z3::add(const BoolExpr &e) {
    if (unsatCores) {
        const std::string &name = "m" + std::to_string(markerCount);
        const z3::expr &m = z3Ctx.bool_const(name.c_str());
        marker.push_back(m);
        uint idx = marker.size() - 1;
        markerMap[name] = idx;
        ++markerCount;
        const z3::expr &imp = z3::implies(m, ExprToSmt<z3::expr>::convert(e, ctx, varMan));
        solver.add(imp);
        return idx;
    } else {
        solver.add(ExprToSmt<z3::expr>::convert(e, ctx, varMan));
        return 0;
    }
}

void Z3::push() {
    solver.push();
    if (unsatCores) {
        markerStack.push(marker.size());
    }
}

void Z3::pop() {
    solver.pop();
    if (unsatCores) {
        marker.resize(markerStack.top());
    }
}

SatResult Z3::check() {
    z3::check_result res = unsatCores ? solver.check(marker) : solver.check();
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
    return Model(vars);
}

Subs Z3::modelSubs() {
    assert(models);
    const z3::model &m = solver.get_model();
    Subs res;
    for (const auto &p: ctx.getSymbolMap()) {
        res.put(p.first, getRealFromModel(m, p.second));
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

void Z3::enableUnsatCores() {
    this->unsatCores = true;
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

std::vector<uint> Z3::unsatCore() {
    const z3::expr_vector &core = solver.unsat_core();
    std::vector<uint> res;
    for (const z3::expr &e: core) {
        res.push_back(markerMap[e.to_string()]);
    }
    return res;
}

void Z3::resetSolver() {
    marker = z3::expr_vector(z3Ctx);
    markerCount = 0;
    markerStack = std::stack<uint>();
    markerMap.clear();
    solver.reset();
    updateParams();
}

BoolExpr Z3::simplify(const BoolExpr &expr, const VariableManager &varMan) {
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
