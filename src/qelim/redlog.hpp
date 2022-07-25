#ifndef REDLOG_HPP
#define REDLOG_HPP

extern "C" {
    #include <reduce.h>
}
#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "../util/exceptions.hpp"
#include "qelim.hpp"

class Redlog: Qelim {

    static RedProc process();
    VariableManager &varMan;

public:

    EXCEPTION(RedlogError, CustomException);

    Redlog(VariableManager &varMan): varMan(varMan){}

    option<BoolExpr> qe(const QuantifiedFormula &qf) override;
    static void init();
    static void exit();

};

#endif
