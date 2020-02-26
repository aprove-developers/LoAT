#ifndef Z3_HPP
#define Z3_HPP

#include "../smt.hpp"
#include "z3context.hpp"

class Z3 : public Smt {

public:
    Z3(const VariableManager &varMan);

    void add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    Result check() override;
    ExprSymbolMap<GiNaC::numeric> model() override;
    void setTimeout(unsigned int timeout) override;
    void resetSolver() override;
    ~Z3() override;

    std::ostream& print(std::ostream& os) const;

private:
    unsigned int timeout;
    Z3Context ctx;
    const VariableManager &varMan;
    z3::solver solver;

    z3::expr convert(const BoolExpr &e);
    GiNaC::numeric getRealFromModel(const z3::model &model, const z3::expr &symbol);

};

#endif // Z3_HPP
