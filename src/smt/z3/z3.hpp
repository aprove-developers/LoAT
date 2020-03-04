#ifdef HAS_Z3

#ifndef Z3_HPP
#define Z3_HPP

#include "../smt.hpp"
#include "z3context.hpp"
#include "../../config.hpp"

class Z3 : public Smt {

public:
    Z3(const VariableManager &varMan);

    void add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    Result check() override;
    ExprSymbolMap<GiNaC::numeric> model() override;
    void setTimeout(unsigned int timeout) override;
    void enableModels() override;
    void resetSolver() override;
    ~Z3() override;

    std::ostream& print(std::ostream& os) const;

private:
    bool models = false;
    unsigned int timeout = Config::Z3::DefaultTimeout;
    const VariableManager &varMan;
    z3::context z3Ctx;
    Z3Context ctx;
    z3::solver solver;

    GiNaC::numeric getRealFromModel(const z3::model &model, const z3::expr &symbol);
    void updateParams();

};

#endif // Z3_HPP

#endif
