#ifndef Z3_HPP
#define Z3_HPP

#include "../smt.hpp"
#include "z3context.hpp"
#include "../../config.hpp"

class Z3 : public Smt {

public:
    Z3(const VariableManager &varMan);

    uint add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    Result check() override;
    Model model() override;
    Subs modelSubs() override;
    void setTimeout(unsigned int timeout) override;
    void enableModels() override;
    void enableUnsatCores() override;
    void resetSolver() override;
    std::vector<uint> unsatCore() override;
    ~Z3() override;

    std::ostream& print(std::ostream& os) const;

    static BoolExpr simplify(const BoolExpr &expr, const VariableManager &varMan);

private:
    bool models = false;
    bool unsatCores = false;
    unsigned int timeout = Config::Smt::DefaultTimeout;
    const VariableManager &varMan;
    z3::context z3Ctx;
    Z3Context ctx;
    z3::solver solver;
    uint markerCount = 0;
    z3::expr_vector marker;
    std::stack<uint> markerStack;
    std::map<std::string, uint> markerMap;

    GiNaC::numeric getRealFromModel(const z3::model &model, const z3::expr &symbol);
    void updateParams();

};

#endif // Z3_HPP
