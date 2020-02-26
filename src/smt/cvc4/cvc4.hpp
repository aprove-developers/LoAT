#ifndef CVC4_HPP
#define CVC4_HPP

#include "../smt.hpp"
#include "cvc4context.hpp"
#include <cvc4/cvc4.h>

class Cvc4: public Smt
{
public:
    Cvc4(const VariableManager &varMan);

    void add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    Result check() override;
    ExprSymbolMap<GiNaC::numeric> model() override;
    void setTimeout(unsigned int timeout) override;
    void resetSolver() override;
    ~Cvc4() override;

private:
    uint timeout;
    Cvc4Context ctx;
    const VariableManager &varMan;
    CVC4::SmtEngine solver;

    CVC4::Expr convert(const BoolExpr &exp);
    GiNaC::numeric getRealFromModel(const CVC4::Expr &symbol);
};

#endif // CVC4_HPP
