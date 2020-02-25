#include "smtfactory.hpp"
#include "z3.hpp"

std::unique_ptr<Smt> SmtFactory::solver(const VariableManager &varMan) {
    return std::unique_ptr<Smt>(new Z3(varMan));
}
