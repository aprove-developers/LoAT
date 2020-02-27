#ifndef SMTFACTORY_HPP
#define SMTFACTORY_HPP

#include "smt.hpp"

class SmtFactory {

public:
    static std::unique_ptr<Smt> solver(Smt::Logic logic, const VariableManager &varMan);
    static std::unique_ptr<Smt> modelBuildingSolver(Smt::Logic logic, const VariableManager &varMan);

};

#endif // SMTFACTORY_HPP
