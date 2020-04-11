#ifndef YICES_HPP
#define YICES_HPP

#include "../smt.hpp"
#include "yicescontext.hpp"
#include "../../config.hpp"
#include "../../sat/yices/yices.hpp"

#include <mutex>

class Yices : public Smt {

    friend class sat::Yices;

public:
    Yices(const VariableManager &varMan, Logic logic);

    uint add(const BoolExpr &e) override;
    void push() override;
    void pop() override;
    SatResult check() override;
    Model model() override;
    Subs modelSubs() override;
    void setTimeout(unsigned int timeout) override;
    void enableModels() override;
    void enableUnsatCores() override;
    void resetSolver() override;
    std::vector<uint> unsatCore() override;
    ~Yices() override;

    std::ostream& print(std::ostream& os) const;

private:
    unsigned int timeout = Config::Smt::DefaultTimeout;
    YicesContext ctx;
    const VariableManager &varMan;
    ctx_config_t *config;
    context_t *solver;
    smt_status res;
    bool unsatCores = false;
    std::vector<term_t> assumptions;
    std::stack<uint> assumptionStack;
    std::map<term_t, uint> assumptionMap;

    GiNaC::numeric getRealFromModel(model_t *model, type_t symbol);

};

#endif // YICES_HPP
