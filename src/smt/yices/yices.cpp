#include "yices.hpp"
#include "../exprtosmt.hpp"
#include "../../util/exceptions.hpp"
#include "../smttoexpr.hpp"

#include <future>
#include <chrono>

Yices::~Yices() {
    mutex.lock();
    --running;
    mutex.unlock();
    yices_free_config(config);
    yices_free_context(solver);
}

Yices::Yices(const VariableManager &varMan, Logic logic): ctx(YicesContext()), varMan(varMan), config(yices_new_config()) {
    if (logic == Smt::QF_NA) {
        yices_set_config(config, "solver-type", "mcsat");
    }
    solver = yices_new_context(config);
    mutex.lock();
    ++running;
    mutex.unlock();
}

void Yices::_add(const BoolExpr &e) {
    term_t converted = ExprToSmt<term_t>::convert(e, ctx, varMan);
    if (yices_assert_formula(solver, converted) < 0) {
        throw YicesError();
    }
}

void Yices::_push() {
    yices_push(solver);
}

void Yices::_pop() {
    yices_pop(solver);
}

Smt::Result Yices::check() {
    std::future<smt_status> future;
    if (unsatCores) {
        std::vector<term_t> assumptions;
        for (const BoolExpr &m: marker) {
            assumptions.push_back(ExprToSmt<term_t>::convert(m, ctx, varMan));
        }
        future = std::async(yices_check_context_with_assumptions, solver, nullptr, assumptions.size(), &assumptions[0]);
    } else {
        future = std::async(yices_check_context, solver, nullptr);
    }
    if (future.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::timeout) {
        switch (future.get()) {
        case STATUS_SAT:
            return Sat;
        case STATUS_UNSAT:
            return Unsat;
        default:
            return Unknown;
        }
    } else {
       yices_stop_search(solver);
       return Unknown;
    }
}

Model Yices::model() {
    model_t *m = yices_get_model(solver, true);
    VarMap<GiNaC::numeric> vars;
    for (const auto &p: ctx.getSymbolMap()) {
        vars[p.first] = getRealFromModel(m, p.second);
    }
    std::map<uint, bool> constants;
    for (const auto &p: ctx.getConstMap()) {
        int32_t val;
        if (yices_get_bool_value(m, p.second, &val) != 0) {
            throw YicesError();
        }
        constants[p.first] = val;
    }
    yices_free_model(m);
    return Model(vars, constants);
}

std::vector<uint> Yices::unsatCore() {
    term_vector_t core;
    yices_init_term_vector(&core);
    if (yices_get_unsat_core(solver, &core) != 0) {
        throw YicesError();
    }
    std::vector<uint> res;
    for (size_t i = 0; i < core.size; ++i) {
        res.push_back(markerMap[SmtToExpr<term_t>::convert(core.data[i], ctx)]);
    }
    return res;
}

GiNaC::numeric Yices::getRealFromModel(model_t *model, type_t symbol) {
    int64_t num;
    uint64_t denom;
    yices_get_rational64_value(model, symbol, &num, &denom);
    assert(denom != 0);
    GiNaC::numeric res = num;
    res = res / denom;
    return res;
}

void Yices::_resetSolver() {
    yices_reset_context(solver);
}

void Yices::_resetContext() {
    ctx.reset();
}

void Yices::updateParams() {
    // do nothing
}

uint Yices::running;
std::mutex Yices::mutex;

void Yices::init() {
    yices_init();
}

void Yices::exit() {
    mutex.lock();
    if (running == 0) {
        yices_exit();
    }
    mutex.unlock();
}
