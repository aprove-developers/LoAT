#ifndef Z3_HPP
#define Z3_HPP

#include "../smt.hpp"
#include "z3context.hpp"
#include "../../config.hpp"

class Z3 : public Smt {

public:
    Z3(const VariableManager &varMan);

    Result check() override;
    Model model() override;
    std::vector<uint> unsatCore() override;
    ~Z3() override;

    std::ostream& print(std::ostream& os) const;

    static BoolExpr simplify(const BoolExpr &expr, const VariableManager &varMan);

private:
    const VariableManager &varMan;
    z3::context z3Ctx;
    Z3Context ctx;
    z3::solver solver;

    GiNaC::numeric getRealFromModel(const z3::model &model, const z3::expr &symbol);
    void _add(const BoolExpr &e) override;
    void _push() override;
    void _pop() override;
    void _resetSolver() override;
    void _resetContext() override;
    void updateParams() override;

};

#endif // Z3_HPP
