#ifndef CVC4CONTEXT_HPP
#define CVC4CONTEXT_HPP

#include <cvc4/cvc4.h>
#include "../../its/types.hpp"

class Cvc4Context: public CVC4::ExprManager
{

public:

    option<CVC4::Expr> getVariable(const ExprSymbol &symbol) const;
    CVC4::Expr addNewVariable(const ExprSymbol &symbol, Expression::Type type = Expression::Int);
    ExprSymbolMap<CVC4::Expr> getSymbolMap() const;

private:

    ExprSymbolMap<CVC4::Expr> symbolMap;
    std::map<std::string,int> usedNames;

    CVC4::Expr generateFreshVar(const std::string &basename, Expression::Type type);

};

#endif // CVC4CONTEXT_HPP
