#ifndef SMT_H
#define SMT_H

#include "../its/types.hpp"
#include "../expr/boolexpr.hpp"

namespace smt {

    enum Result {Sat, Unknown, Unsat};
    enum Logic {LA, NA, ENA};

}

template<class EXPR>
class Smt
{
public:

    virtual bool add(const BoolExpr &e) = 0;

    bool add(const Rel &e) {
        return this->add(buildLit(e));
    }

    virtual void push() = 0;
    virtual void pop() = 0;
    virtual smt::Result check() = 0;
    virtual VarMap<GiNaC::numeric> model() = 0;
    virtual void setTimeout(unsigned int timeout) = 0;
    virtual void enableModels() = 0;
    virtual void resetSolver() = 0;

    virtual ~Smt() {}

    virtual EXPR getInt(long val) = 0;
    virtual EXPR getReal(long num, long denom) = 0;
    virtual EXPR pow(const EXPR &base, const EXPR &exp) = 0;
    virtual EXPR plus(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR times(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR eq(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR lt(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR le(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR gt(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR ge(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR neq(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR bAnd(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR bOr(const EXPR &x, const EXPR &y) = 0;
    virtual EXPR bTrue() = 0;
    virtual EXPR bFalse() = 0;
    virtual EXPR var(const std::string &basename, Expr::Type type) = 0;

    option<EXPR> getVariable(const Var &symbol) const {
        auto it = symbolMap.find(symbol);
        if (it != symbolMap.end()) {
            return it->second;
        }
        return {};
    }

    EXPR addNewVariable(const Var &symbol, Expr::Type type = Expr::Int) {
        assert(symbolMap.count(symbol) == 0);
        EXPR res = generateFreshVar(symbol.get_name(), type);
        symbolMap.emplace(symbol, res);
        return res;
    }

protected:
    VarMap<EXPR> getSymbolMap() const {
        return symbolMap;
    }

    VarMap<EXPR> symbolMap;
    std::map<std::string, int> usedNames;

    EXPR generateFreshVar(const std::string &basename, Expr::Type type) {
        std::string newname = basename;

        while (usedNames.find(newname) != usedNames.end()) {
            int cnt = usedNames[basename]++;
            newname = basename + "_" + std::to_string(cnt);
        }

        usedNames.emplace(newname, 1); // newname is now used once
        return var(newname, type);
    }

};

#endif // SMT_H
