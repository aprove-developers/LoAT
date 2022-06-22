#ifndef REDLOG_HPP
#define REDLOG_HPP

#include "../../include/reduce.h"
#include "../expr/boolexpr.hpp"

class Redlog {

    static RedProc process;

public:

    option<BoolExpr> qe(const QuantifiedFormula &qf);
    static void init();
    static void exit();

};

#endif
