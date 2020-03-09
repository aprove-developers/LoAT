#ifdef HAS_YICES

#ifndef YICES_HPP
#define YICES_HPP

#include "../smt.hpp"
#include "../../config.hpp"

#include <mpfi.h>
#include <yices.h>
#include <mutex>

class Yices : public Smt<term_t> {

public:
    Yices(const VariableManager &varMan, smt::Logic logic);

    bool add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    smt::Result check() override;
    smt::Result _check();
    VarMap<GiNaC::numeric> model() override;
    void setTimeout(unsigned int timeout) override;
    void enableModels() override;
    void resetSolver() override;
    ~Yices() override;

    term_t getInt(long val) override;
    term_t getReal(long num, long denom) override;
    term_t pow(const term_t &base, const term_t &exp) override;
    term_t plus(const term_t &x, const term_t &y) override;
    term_t times(const term_t &x, const term_t &y) override;
    term_t eq(const term_t &x, const term_t &y) override;
    term_t lt(const term_t &x, const term_t &y) override;
    term_t le(const term_t &x, const term_t &y) override;
    term_t gt(const term_t &x, const term_t &y) override;
    term_t ge(const term_t &x, const term_t &y) override;
    term_t neq(const term_t &x, const term_t &y) override;
    term_t bAnd(const term_t &x, const term_t &y) override;
    term_t bOr(const term_t &x, const term_t &y) override;
    term_t bTrue() override;
    term_t bFalse() override;
    term_t var(const std::string &basename, Expr::Type type) override;

    static void init();
    static void exit();

    std::ostream& print(std::ostream& os) const;

private:
    unsigned int timeout = Config::Z3::DefaultTimeout;
    const VariableManager &varMan;
    ctx_config_t *config;
    context_t *solver;
    smt_status res;

    static uint running;
    static std::mutex mutex;


    GiNaC::numeric getRealFromModel(model_t *model, type_t symbol);

};

#endif // YICES_HPP

#endif
