#include "cvc4context.hpp"

option<CVC4::Expr> Cvc4Context::getVariable(const ExprSymbol &symbol) const {
    auto it = symbolMap.find(symbol);
    if (it != symbolMap.end()) {
        return it->second;
    }
    return {};
}

CVC4::Expr Cvc4Context::addNewVariable(const ExprSymbol &symbol, Expression::Type type) {
    // This symbol must not have been mapped to a z3 variable before (can be checked via getVariable)
    assert(symbolMap.count(symbol) == 0);

    // Associated the GiNaC symbol with the resulting variable
    CVC4::Expr res = generateFreshVar(symbol.get_name(), type);
    symbolMap.emplace(symbol, res);
    return res;
}

CVC4::Expr Cvc4Context::generateFreshVar(const std::string &basename, Expression::Type type) {
    std::string newname = basename;

    while (usedNames.find(newname) != usedNames.end()) {
        int cnt = usedNames[basename]++;
        newname = basename + "_" + std::to_string(cnt);
    }

    usedNames.emplace(newname, 1); // newname is now used once
    return (type == Expression::Int) ? mkVar(newname.c_str(), integerType()) : mkVar(newname.c_str(), realType());
}

ExprSymbolMap<CVC4::Expr> Cvc4Context::getSymbolMap() const {
    return symbolMap;
}
