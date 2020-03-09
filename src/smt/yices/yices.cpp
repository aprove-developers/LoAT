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

Yices::Yices(const VariableManager &varMan, smt::Logic logic): varMan(varMan), config(yices_new_config()) {
    if (logic == smt::NA) {
        yices_set_config(config, "solver-type", "mcsat");
    }
    solver = yices_new_context(config);
    mutex.lock();
    ++running;
    mutex.unlock();
//    std::cerr << "new solver" << std::endl;
}

bool Yices::add(const BoolExpr &e) {
    term_t converted = GinacToSmt<term_t>::convert(e, *this, varMan);
//    yices_pp_term(stderr, converted, 80, 20, 0);
    if (yices_assert_formula(solver, converted) < 0) {
        error_code_t error = yices_error_code();
        if (error == CTX_NONLINEAR_ARITH_NOT_SUPPORTED || error == MCSAT_ERROR_UNSUPPORTED_THEORY) {
            yices_clear_error();
            return false;
        }
        std::cerr << "yices error" << std::endl;
        throw YicesError();
    }
    return true;
}

void Yices::push() {
    yices_push(solver);
}

void Yices::pop() {
    yices_pop(solver);
}

smt::Result Yices::check() {
//    std::cerr << "check " << timeout << std::endl;
    auto res = _check();
//    std::cerr << "done" << std::endl;
    return res;
}

smt::Result Yices::_check() {
    auto future = std::async(yices_check_context, solver, nullptr);
    if (future.wait_for(std::chrono::milliseconds(timeout)) != std::future_status::timeout) {
        switch (future.get()) {
        case STATUS_SAT:
            return smt::Sat;
        case STATUS_UNSAT:
            return smt::Unsat;
        default:
            return smt::Unknown;
        }
    } else {
       yices_stop_search(solver);
       return smt::Unknown;
    }
}

VarMap<GiNaC::numeric> Yices::model() {
    model_t *m = yices_get_model(solver, true);
    VarMap<GiNaC::numeric> res;
    for (const auto &p: symbolMap) {
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

term_t Yices::var(const std::string &name, Expr::Type type) {
    term_t res = (type == Expr::Int) ? yices_new_uninterpreted_term(yices_int_type()) : yices_new_uninterpreted_term(yices_real_type());
    yices_set_term_name(res, name.c_str());
    return res;
}

term_t Yices::getInt(long val) {
    return yices_int64(val);
}

term_t Yices::getReal(long num, long denom) {
    assert(denom != 0);
    if (denom > 0) {
        return yices_rational64(num, denom);
    } else {
        return yices_rational64(-num, -denom);
    }
}

term_t Yices::pow(const term_t &base, const term_t &exp) {
    mpq_t q;
    mpz_t num, denom;
    mpq_init(q);
    int error = yices_rational_const_value(exp, q);
    assert(error == 0);
    mpq_get_num(denom, q);
    assert(mpz_get_si(denom) == 1);
    mpq_get_num(num, q);
    return yices_power(base, mpz_get_ui(num));
}

term_t Yices::plus(const term_t &x, const term_t &y) {
    return yices_add(x, y);
}

term_t Yices::times(const term_t &x, const term_t &y) {
    return yices_mul(x, y);
}

term_t Yices::eq(const term_t &x, const term_t &y) {
    return yices_arith_eq_atom(x, y);
}

term_t Yices::lt(const term_t &x, const term_t &y) {
    return yices_arith_lt_atom(x, y);
}

term_t Yices::le(const term_t &x, const term_t &y) {
    return yices_arith_leq_atom(x, y);
}

term_t Yices::gt(const term_t &x, const term_t &y) {
    return yices_arith_gt_atom(x, y);
}

term_t Yices::ge(const term_t &x, const term_t &y) {
    return yices_arith_geq_atom(x, y);
}

term_t Yices::neq(const term_t &x, const term_t &y) {
    return yices_arith_neq_atom(x, y);
}

term_t Yices::bAnd(const term_t &x, const term_t &y) {
    return yices_and2(x, y);
}

term_t Yices::bOr(const term_t &x, const term_t &y) {
    return yices_or2(x, y);
}

term_t Yices::bTrue() {
    return yices_true();
}

term_t Yices::bFalse() {
    return yices_false();
}

#endif
