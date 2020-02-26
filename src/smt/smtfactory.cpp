#include "smtfactory.hpp"
#include "cvc4/cvc4.hpp"

std::unique_ptr<Smt> SmtFactory::solver(const VariableManager &varMan) {
    return std::unique_ptr<Smt>(new Cvc4(varMan));
}
