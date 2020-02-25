#ifndef SMTFACTORY_HPP
#define SMTFACTORY_HPP

#include "smt.hpp"

class SmtFactory {

public:

    static std::unique_ptr<Smt> solver(const VariableManager &varMan);

};

#endif // SMTFACTORY_HPP
