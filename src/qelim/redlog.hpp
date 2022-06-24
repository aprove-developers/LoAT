#ifndef REDLOG_HPP
#define REDLOG_HPP

extern "C" {
    #include <reduce.h>
}
#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "../util/exceptions.hpp"

class Redlog {

    Redlog();
    static RedProc process();

public:

    EXCEPTION(RedlogError, CustomException);

    static option<BoolExpr> qe(const QuantifiedFormula &qf, VariableManager &varMan);
    static void init();
    static void exit();

};

#endif
