#ifdef HAS_CVC4

#ifndef CVC4_HPP
#define CVC4_HPP

#include "../smt.hpp"
#include <cvc4/cvc4.h>

class Cvc4: public Smt<CVC4::Expr>
{
public:
    Cvc4(const VariableManager &varMan);

    bool add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    smt::Result check() override;
    VarMap<GiNaC::numeric> model() override;
    void setTimeout(unsigned int timeout) override;
    void enableModels() override;
    void resetSolver() override;
    ~Cvc4() override;

    CVC4::Expr getInt(long val) override;
    CVC4::Expr getReal(long num, long denom) override;
    CVC4::Expr pow(const CVC4::Expr &base, const CVC4::Expr &exp) override;
    CVC4::Expr plus(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr times(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr eq(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr lt(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr le(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr gt(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr ge(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr neq(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr bAnd(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr bOr(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr bTrue() override;
    CVC4::Expr bFalse() override;
    CVC4::Expr var(const std::string &basename, Expr::Type type) override;

private:
    uint timeout;
    const VariableManager &varMan;
    CVC4::ExprManager manager;
    CVC4::SmtEngine solver;
    bool models = false;

    GiNaC::numeric getRealFromModel(const CVC4::Expr &symbol);
};

#endif // CVC4_HPP

#endif
