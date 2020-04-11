#ifndef SATFACTORY_HPP
#define SATFACTORY_HPP

#include "../config.hpp"
#include "sat.hpp"
#include <memory>

namespace sat {

class SatFactory
{

public:
    static std::unique_ptr<Sat> solver(uint timeout = Config::Smt::DefaultTimeout);

};

}

#endif // SATFACTORY_HPP
