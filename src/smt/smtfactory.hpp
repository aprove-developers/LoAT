#ifndef SMTFACTORY_HPP
#define SMTFACTORY_HPP

#include "smt.hpp"
#include "../config.hpp"

class SmtFactory {

public:
    static std::unique_ptr<Smt> solver(Smt::Logic logic, VariableManager &varMan, uint timeout = Config::Smt::DefaultTimeout);
    static std::unique_ptr<Smt> modelBuildingSolver(Smt::Logic logic, VariableManager &varMan, uint timeout = Config::Smt::DefaultTimeout);

};

#endif // SMTFACTORY_HPP
