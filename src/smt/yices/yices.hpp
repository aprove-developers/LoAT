#ifdef HAS_YICES

#ifndef YICES_HPP
#define YICES_HPP

#include "../smt.hpp"
#include "yicescontext.hpp"
#include "../../config.hpp"

class Yices : public Smt {

public:
    Yices(const VariableManager &varMan);

    void add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    Result check() override;
    ExprSymbolMap<GiNaC::numeric> model() override;
    void setTimeout(unsigned int timeout) override;
    void enableModels() override;
    void resetSolver() override;
    ~Yices() override;

    std::ostream& print(std::ostream& os) const;

private:
    bool models = false;
    unsigned int timeout = Config::Z3::DefaultTimeout;
    YicesContext ctx;
    const VariableManager &varMan;
    context_t *solver;
    smt_status res;

    term_t convert(const BoolExpr &e);
    GiNaC::numeric getRealFromModel(model_t *model, type_t symbol);

};

#endif // YICES_HPP

#endif
