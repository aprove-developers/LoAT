#include "term.hpp"
#include "../../expr/rel.hpp"

using namespace parser;


bool TermBinOp::isArithmeticExpression() const {
    return lhs->isArithmeticExpression() && rhs->isArithmeticExpression();
}

void TermBinOp::collectVariables(VarSet &set) const {
    lhs->collectVariables(set);
    rhs->collectVariables(set);
}

Expr TermBinOp::toGinacExpression() const {
    Expr l = lhs->toGinacExpression();
    Expr r = rhs->toGinacExpression();

    switch (op) {
        case Addition: return l + r;
        case Subtraction: return l - r;
        case Multiplication: return l * r;
        case Division: return l / r;
        case Power: return l ^ r;
    }
    assert(false && "unreachable");
}


bool TermFunApp::isFunappOnArithmeticExpressions() const {
    return std::all_of(args.cbegin(), args.cend(), [](const TermPtr &t){ return t->isArithmeticExpression(); });
}

void TermFunApp::collectVariables(VarSet &set) const {
    for (const TermPtr &arg : args) {
        arg->collectVariables(set);
    }
}

Expr TermFunApp::toGinacExpression() const {
    throw CannotConvertToGinacException("Cannot convert function symbol to GiNaC: "+name);
}


Rel Relation::toGinacExpression() const {
    Expr l = lhs->toGinacExpression();
    Expr r = rhs->toGinacExpression();

    switch (op) {
    case RelationEqual: return Rel::buildEq(l, r);
    case RelationNotEqual: return Rel::buildNeq(l, r);
    case RelationGreater: return l > r;
    case RelationGreaterEqual: return l >= r;
    case RelationLess: return l < r;
    case RelationLessEqual: return l <= r;
    }
    assert(false && "unreachable");
}
