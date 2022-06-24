#ifndef QEPCAD_HPP
#define QEPCAD_HPP

#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "../parser/qepcad/qepcadparsevisitor.h"

#include <qepcad/qepcad.h>

class Qepcad
{
public:
    static option<BoolExpr> qe(const QuantifiedFormula &qf, VariableManager &varMan);
};

#endif // QEPCAD_HPP
