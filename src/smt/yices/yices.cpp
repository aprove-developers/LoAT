#ifdef HAS_YICES

#include "yices.hpp"
#include "../ginactosmt.hpp"
#include "../../util/exceptions.hpp"

#include <future>
#include <chrono>

EXCEPTION(YicesError, CustomException);

Yices::~Yices() {
    mutex.lock();
    --running;
    mutex.unlock();
    yices_free_config(config);
    yices_free_context(solver);
}

Yices::Yices(const VariableManager &varMan): ctx(YicesContext()), varMan(varMan), config(yices_new_config()) {
    yices_set_config(config, "solver-type", "mcsat");
    solver = yices_new_context(config);
    mutex.lock();
    ++running;
    mutex.unlock();
}

void Yices::add(const BoolExpr &e) {
    term_t converted = GinacToSmt<term_t>::convert(e, ctx, varMan);
    if (yices_assert_formula(solver, converted) < 0) {
        throw YicesError();
    }
}

void Yices::push() {
    yices_push(solver);
}

void Yices::pop() {
    yices_pop(solver);
}

Smt::Result Yices::check() {
    auto future = std::async(yices_check_context, solver, nullptr);
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

ExprSymbolMap<GiNaC::numeric> Yices::model() {
    model_t *m = yices_get_model(solver, true);
    ExprSymbolMap<GiNaC::numeric> res;
    for (const auto &p: ctx.getSymbolMap()) {
        res[p.first] = getRealFromModel(m, p.second);
    }
    yices_free_model(m);
    return res;
}

void Yices::setTimeout(unsigned int timeout) {
    this->timeout = timeout;
}

void Yices::enableModels() { }

GiNaC::numeric Yices::getRealFromModel(model_t *model, type_t symbol) {
    int64_t num;
    uint64_t denom;
    yices_get_rational64_value(model, symbol, &num, &denom);
    assert(denom != 0);
    GiNaC::numeric res = num;
    res = res / denom;
    return res;
}

void Yices::resetSolver() {
    yices_free_context(solver);
    solver = yices_new_context(config);
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

#endif
