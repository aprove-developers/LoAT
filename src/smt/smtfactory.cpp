#include "smtfactory.hpp"
#include "z3.hpp"

std::unique_ptr<Smt> SmtFactory::solver() {
    return std::unique_ptr<Smt>(new Z3());
}
