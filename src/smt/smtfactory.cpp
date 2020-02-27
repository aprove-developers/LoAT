#include "smtfactory.hpp"
#include "../config.hpp"
#ifdef HAS_CVC4
#include "cvc4/cvc4.hpp"
#endif
#ifdef HAS_Z3
#include "z3/z3.hpp"
#endif

std::unique_ptr<Smt> SmtFactory::solver(const VariableManager &varMan) {
    switch (Config::Z3::solver) {
    case Config::Z3::cvc4:
#ifdef HAS_CVC4
        return std::unique_ptr<Smt>(new Cvc4(varMan));
#endif
    case Config::Z3::z3:
#ifdef HAS_Z3
        return std::unique_ptr<Smt>(new Z3(varMan));
#endif
    }
}
