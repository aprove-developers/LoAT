#ifndef YICES_HPP
#define YICES_HPP

#include "../smt.hpp"
#include "yicescontext.hpp"
#include "../../config.hpp"

#include <mutex>

class Yices : public Smt {

public:
    Yices(const VariableManager &varMan, Logic logic);

    Result check() override;
    Model model() override;
    std::vector<uint> unsatCore() override;
    ~Yices() override;

    static void init();
    static void exit();

    std::ostream& print(std::ostream& os) const;

private:
    YicesContext ctx;
    const VariableManager &varMan;
    ctx_config_t *config;
    context_t *solver;

    static uint running;
    static std::mutex mutex;

    void _add(const BoolExpr &e) override;
    void _push() override;
    void _pop() override;
    void _resetSolver() override;
    void _resetContext() override;
    void updateParams() override;
    GiNaC::numeric getRealFromModel(model_t *model, type_t symbol);

};

#endif // YICES_HPP
