#ifndef SMTFACTORY_HPP
#define SMTFACTORY_HPP

#include "smt.hpp"
#include "../config.hpp"

class SmtFactory {

public:
    static std::unique_ptr<Smt> solver(Smt::Logic logic, const VariableManager &varMan, uint timeout = Config::Z3::DefaultTimeout, bool verbose = false);
    static std::unique_ptr<Smt> modelBuildingSolver(Smt::Logic logic, const VariableManager &varMan, uint timeout = Config::Z3::DefaultTimeout);

};

#endif // SMTFACTORY_HPP
