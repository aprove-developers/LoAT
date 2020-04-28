#ifndef CVC4_HPP
#define CVC4_HPP

#include "../smt.hpp"
#include "cvc4context.hpp"
#include <cvc4/cvc4.h>

class Cvc4: public Smt
{
public:
    Cvc4(const VariableManager &varMan);

    Result check() override;
    Model model() override;
    std::vector<uint> unsatCore() override;
    ~Cvc4() override;

private:
    const VariableManager &varMan;
    CVC4::ExprManager manager;
    Cvc4Context ctx;
    CVC4::SmtEngine solver;

    void _add(const ForAllExpr &e) override;
    void _push() override;
    void _pop() override;
    void _resetSolver() override;
    void updateParams() override;
    GiNaC::numeric getRealFromModel(const CVC4::Expr &symbol);
};

#endif // CVC4_HPP
