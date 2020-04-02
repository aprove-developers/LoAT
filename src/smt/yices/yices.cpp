#ifdef HAS_YICES

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
    if (logic == Smt::NA) {
        yices_set_config(config, "solver-type", "mcsat");
    }
    solver = yices_new_context(config);
    mutex.lock();
    ++running;
    mutex.unlock();
}

void Yices::add(const BoolExpr &e) {
    term_t converted = ExprToSmt<term_t>::convert(e, ctx, varMan);
    if (unsatCores) {
        assumptions.push_back(converted);
        assumptionMap[converted] = e;
    } else {
        if (yices_assert_formula(solver, converted) < 0) {
            throw YicesError();
        }
    }
}

void Yices::push() {
    yices_push(solver);
    if (unsatCores) {
        assumptionStack.push(assumptions.size());
    }
}

void Yices::pop() {
    yices_pop(solver);
    if (unsatCores) {
        assumptions.resize(assumptionStack.top());
        assumptionStack.pop();
    }
}

Smt::Result Yices::check() {
    auto future = unsatCores ?
                std::async(yices_check_context_with_assumptions, solver, nullptr, assumptions.size(), &assumptions[0]) :
                std::async(yices_check_context, solver, nullptr);
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

BoolExpr Yices::unsatCore() {
    term_vector_t core;
    yices_init_term_vector(&core);
    if (yices_get_unsat_core(solver, &core) != 0) {
        throw YicesError();
    }
    std::vector<BoolExpr> res;
    for (uint i = 0; i < core.size; ++i) {
        res.push_back(assumptionMap[core.data[i]]);
    }
    return buildAnd(res);
}

Subs Yices::modelSubs() {
    model_t *m = yices_get_model(solver, true);
    Subs res;
    for (const auto &p: ctx.getSymbolMap()) {
        res.put(p.first, getRealFromModel(m, p.second));
    }
    yices_free_model(m);
    return res;
}

void Yices::setTimeout(unsigned int timeout) {
    this->timeout = timeout;
}

void Yices::enableModels() { }

void Yices::enableUnsatCores() {
    unsatCores = true;
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

void Yices::resetSolver() {
    assumptions.clear();
    assumptionStack = std::stack<uint>();
    assumptionMap.clear();
    yices_reset_context(solver);
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
