#ifndef REDLOG_HPP
#define REDLOG_HPP

extern "C" {
    #include <reduce.h>
}
#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"

class Redlog {

    Redlog();
    static RedProc process();

public:

    static option<BoolExpr> qe(const QuantifiedFormula &qf, VariableManager &varMan);
    static void init();
    static void exit();

};

#endif
