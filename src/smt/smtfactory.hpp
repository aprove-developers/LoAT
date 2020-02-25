#ifndef SMTFACTORY_HPP
#define SMTFACTORY_HPP

#include "smt.hpp"

class SmtFactory {

public:

    static std::unique_ptr<Smt> solver();

};

#endif // SMTFACTORY_HPP
