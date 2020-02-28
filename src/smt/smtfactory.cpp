#include "smtfactory.hpp"
#include "../config.hpp"

#ifdef HAS_CVC4
#include "cvc4/cvc4.hpp"
#endif
#ifdef HAS_Z3
#include "z3/z3.hpp"
#endif
#ifdef HAS_YICES
#include "yices/yices.hpp"
#endif

std::unique_ptr<Smt> SmtFactory::solver(Smt::Logic logic, const VariableManager &varMan, uint timeout) {
    std::unique_ptr<Smt> res;
    switch (logic) {
    case Smt::LA:
        if (timeout < 1000) {
            res = std::unique_ptr<Smt>(new Yices(varMan));
        } else {
#if HAS_Z3
            res = std::unique_ptr<Smt>(new Z3(varMan));
#elif HAS_CVC4
            res = std::unique_ptr<Smt>(new Cvc4(varMan));
#else
            res = std::unique_ptr<Smt>(new Yices(varMan));
#endif
        }
        break;
    case Smt::NA:
        if (timeout < 1000) {
            res = std::unique_ptr<Smt>(new Yices(varMan));
        } else {
#if HAS_CVC4
            res = std::unique_ptr<Smt>(new Z3(varMan));
#elif HAS_Z3
            res = std::unique_ptr<Smt>(new Z3(varMan));
#else
            res = std::unique_ptr<Smt>(new Yices(varMan));
#endif
        }
        break;
    case Smt::ENA:
#if HAS_Z3
        res = std::unique_ptr<Smt>(new Z3(varMan));
#elif HAS_YICES
        res = std::unique_ptr<Smt>(new Yices(varMan));
#else
        res = std::unique_ptr<Smt>(new Cvc4(varMan));
#endif
    }
    res->setTimeout(timeout);
}

std::unique_ptr<Smt> SmtFactory::modelBuildingSolver(Smt::Logic logic, const VariableManager &varMan, uint timeout) {
    std::unique_ptr<Smt> res = solver(logic, varMan, timeout);
    res->enableModels();
    return res;
}
