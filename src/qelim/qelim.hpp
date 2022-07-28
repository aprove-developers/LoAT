#ifndef QELIM_HPP
#define QELIM_HPP

#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "../util/exceptions.hpp"

class Qelim {

public:

    struct Result {
        BoolExpr qf;
        bool exact;
        Result(const BoolExpr &qf, bool exact): qf(qf), exact(exact) {}

    };

    virtual option<Result> qe(const QuantifiedFormula &qf) = 0;

    static std::unique_ptr<Qelim> solver(VarMan& varMan);

};

#endif

