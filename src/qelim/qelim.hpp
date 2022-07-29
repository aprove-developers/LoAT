#ifndef QELIM_HPP
#define QELIM_HPP

#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "../util/exceptions.hpp"
#include "../util/proof.hpp"

class Qelim {

public:

    struct Result {
        BoolExpr qf;
        Proof proof;
        bool exact;
        Result(const BoolExpr &qf, const Proof &proof, bool exact): qf(qf), proof(proof), exact(exact) {}
    };

    virtual option<Result> qe(const QuantifiedFormula &qf) = 0;

    static std::unique_ptr<Qelim> solver(VarMan& varMan);

};

#endif

