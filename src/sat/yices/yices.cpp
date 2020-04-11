#include "yices.hpp"
#include "../../util/yicesmanager.hpp"
#include "../../util/yiceserror.hpp"

#include <future>
#include <chrono>

namespace sat {

Yices::Yices(): solver(yices_new_context(nullptr)) {
    YicesManager::inc();
}

Yices::~Yices() {
    YicesManager::dec();
}

void Yices::add(const PropExpr &e) {
    yices_assert_formula(solver, convert(e));
}

term_t Yices::convert(const PropExpr &e) {
    if (e->lit()) {
        int lit = e->lit().get();
        int var = lit >= 0 ? lit : -lit;
        term_t res;
        if (vars.count(var) > 0) {
            res = vars.at(var);
        } else {
            res = yices_new_uninterpreted_term(yices_bool_type());
            vars.emplace(var, res);
        }
        if (lit >= 0) {
            return res;
        } else {
            return yices_neg(res);
        }
    } else {
        const PropExprSet &children = e->getChildren();
        const size_t size = children.size();
        term_t *res = static_cast<term_t*>(malloc(size * sizeof (term_t)));
        auto it = children.begin();
        for (uint i = 0; i < size; ++i, ++it) {
            res[i] = convert(*it);
        }
        term_t ret = e->isAnd() ? yices_and(size, res) : yices_or(size, res);
        delete res;
        return ret;
    }
}

SatResult Yices::check() const {
    auto future = std::async(yices_check_context, solver, nullptr);
    if (future.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::timeout) {
        switch (future.get()) {
        case STATUS_SAT:
            return SatResult::Sat;
        case STATUS_UNSAT:
            return SatResult::Unsat;
        default:
            return SatResult::Unknown;
        }
    } else {
       yices_stop_search(solver);
       return SatResult::Unknown;
    }
}

void Yices::setTimeout(const uint timeout) {
    this->timeout = timeout;
}

Model Yices::model() const {
    model_t *model = yices_get_model(solver, true);
    std::map<uint, bool> res;
    for (const auto &p: vars) {
        int32_t b;
        int32_t error = yices_get_bool_value(model, p.second, &b);
        if (error == 0) {
            res.emplace(p.first, b);
        } else if (error != -1) {
            throw YicesError();
        }
    }
    yices_free_model(model);
    return Model(res);
}

void Yices::push() {
    yices_push(solver);
}

void Yices::pop() {
    yices_pop(solver);
}

}
