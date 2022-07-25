#ifndef QELIM_HPP
#define QELIM_HPP

#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "../util/exceptions.hpp"

class Qelim {

public:

    virtual option<BoolExpr> qe(const QuantifiedFormula &qf) = 0;

    static std::unique_ptr<Qelim> solver();

};

#endif

