#ifndef Z3_HPP
#define Z3_HPP

#include "../smt.hpp"
#include "../../config.hpp"

#include <z3++.h>

class Z3 : public Smt<z3::expr> {

public:
    Z3(const VariableManager &varMan);

    bool add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    smt::Result check() override;
    VarMap<GiNaC::numeric> model() override;
    void setTimeout(unsigned int timeout) override;
    void enableModels() override;
    void resetSolver() override;
    ~Z3() override;

    z3::expr getInt(long val) override;
    z3::expr getReal(long num, long denom) override;
    z3::expr pow(const z3::expr &base, const z3::expr &exp) override;
    z3::expr plus(const z3::expr &x, const z3::expr &y) override;
    z3::expr times(const z3::expr &x, const z3::expr &y) override;
    z3::expr eq(const z3::expr &x, const z3::expr &y) override;
    z3::expr lt(const z3::expr &x, const z3::expr &y) override;
    z3::expr le(const z3::expr &x, const z3::expr &y) override;
    z3::expr gt(const z3::expr &x, const z3::expr &y) override;
    z3::expr ge(const z3::expr &x, const z3::expr &y) override;
    z3::expr neq(const z3::expr &x, const z3::expr &y) override;
    z3::expr bAnd(const z3::expr &x, const z3::expr &y) override;
    z3::expr bOr(const z3::expr &x, const z3::expr &y) override;
    z3::expr bTrue() override;
    z3::expr bFalse() override;
    z3::expr var(const std::string &basename, Expr::Type type) override;

    std::ostream& print(std::ostream& os) const;

private:
    bool models = false;
    unsigned int timeout = Config::Z3::DefaultTimeout;
    const VariableManager &varMan;
    z3::context ctx;
    z3::solver solver;

    GiNaC::numeric getRealFromModel(const z3::model &model, const z3::expr &symbol);
    void updateParams();

};

#endif // Z3_HPP
