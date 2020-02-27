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

std::unique_ptr<Smt> SmtFactory::solver(Smt::Logic logic, const VariableManager &varMan) {
    switch (logic) {
    case Smt::NA:
#ifdef HAS_YICES
        return std::unique_ptr<Smt>(new Yices(varMan));
#elif #defined HAS_Z3
        return std::unique_ptr<Smt>(new Z3(varMan));
#else
        return std::unique_ptr<Smt>(new Cvc4(varMan));
#endif
    case Smt::ENA:
#ifdef HAS_Z3
        return std::unique_ptr<Smt>(new Z3(varMan));
#elif #defined HAS_YICES
        return std::unique_ptr<Smt>(new Yices(varMan));
#else
        return std::unique_ptr<Smt>(new Cvc4(varMan));
#endif
    case Smt::LA:
#ifdef HAS_CVC4
        return std::unique_ptr<Smt>(new Cvc4(varMan));
#elif defined HAS_Z3
        return std::unique_ptr<Smt>(new Z3(varMan));
#else
        return std::unique_ptr<Smt>(new Yices(varMan));
#endif
    }
}

std::unique_ptr<Smt> SmtFactory::modelBuildingSolver(Smt::Logic logic, const VariableManager &varMan) {
    std::unique_ptr<Smt> res = solver(logic, varMan);
    res->enableModels();
    return res;
}
