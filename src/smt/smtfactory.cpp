#include "smtfactory.hpp"
#include "../config.hpp"
#include "z3/z3.hpp"

#ifdef HAS_CVC4
#include "cvc4/cvc4.hpp"
#endif
#ifdef HAS_YICES
#include "yices/yices.hpp"
#endif

std::unique_ptr<Smt> SmtFactory::solver(Smt::Logic logic, const VariableManager &varMan, uint timeout) {
    std::unique_ptr<Smt> res;
    switch (logic) {
    case Smt::LA:
#if HAS_YICES
        res = std::unique_ptr<Smt>(new Yices(varMan, logic));
#else
        res = std::unique_ptr<Smt>(new Z3(varMan));
#endif
        break;
    case Smt::NA:
        res = std::unique_ptr<Smt>(new Z3(varMan));
        break;
    case Smt::ENA:
        res = std::unique_ptr<Smt>(new Z3(varMan));
    }
    res->setTimeout(timeout);
    return res;
}

std::unique_ptr<Smt> SmtFactory::modelBuildingSolver(Smt::Logic logic, const VariableManager &varMan, uint timeout) {
    std::unique_ptr<Smt> res = solver(logic, varMan, timeout);
    res->enableModels();
    return res;
}
