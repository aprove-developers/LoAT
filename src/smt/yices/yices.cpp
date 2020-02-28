#ifdef HAS_YICES

#include "yices.hpp"
#include "../ginactosmt.hpp"
#include <future>
#include <chrono>

Yices::~Yices() {
    yices_free_context(solver);
}

Yices::Yices(const VariableManager &varMan): varMan(varMan), solver(yices_new_context(nullptr)) { }

void Yices::add(const BoolExpr &e) {
    yices_assert_formula(solver, convert(e));
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
        case STATUS_SAT: return Sat;
        case STATUS_UNSAT: return Unsat;
        default: return Unknown;
        }
    } else {
       yices_stop_search(solver);
       return Unknown;
    }
}

ExprSymbolMap<GiNaC::numeric> Yices::model() {
    assert(models);
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

void Yices::enableModels() {
    this->models = true;
}

term_t Yices::convert(const BoolExpr &e) {
    if (e->getLit()) {
        return GinacToSmt<term_t>::convert(e->getLit().get(), ctx, varMan);
    }
    term_t res = e->isAnd() ? yices_true() : yices_false();
    bool first = true;
    for (const BoolExpr &c: e->getChildren()) {
        if (first) {
            res = convert(c);
            first = false;
        } else {
            res = e->isAnd() ? yices_and2(res, convert(c)) : yices_or2(res, convert(c));
        }
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

void Yices::resetSolver() {
    yices_reset_context(solver);
}

#endif
