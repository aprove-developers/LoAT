#ifndef SMT_H
#define SMT_H

#include "../its/types.hpp"
#include "../expr/boolexpr.hpp"

class Smt
{
public:

    enum Result {Sat, Unknown, Unsat};
    enum Logic {LA, NA, ENA};

    virtual void add(const BoolExpr &e) = 0;
    void add(const Expression &e);
    virtual void push() = 0;
    virtual void pop() = 0;
    virtual Result check() = 0;
    virtual ExprSymbolMap<GiNaC::numeric> model() = 0;
    virtual void setTimeout(unsigned int timeout) = 0;
    virtual void enableModels() = 0;
    virtual void resetSolver() = 0;
    virtual ~Smt();

    static option<ExprSymbolMap<GiNaC::numeric>> maxSmt(BoolExpr hard, std::vector<BoolExpr> soft, uint timeout, const VariableManager &varMan);
    static Smt::Result check(const BoolExpr &e, const VariableManager &varMan);
    static bool isImplication(const BoolExpr &lhs, const BoolExpr &rhs, const VariableManager &varMan);
    static Logic chooseLogic(const std::vector<BoolExpr> &xs);

    template<class T> static Logic chooseLogic(const std::vector<std::vector<Expression>> &g, const std::vector<T> &up) {
        if (isLinear(g) && isLinear(up)) {
            return Smt::LA;
        }
        if (isPolynomial(g) && isPolynomial(up)) {
            return Smt::NA;
        }
        return Smt::ENA;
    }

private:
    static bool isLinear(const std::vector<Expression> &guard);
    static bool isLinear(const UpdateMap &up);
    static bool isLinear(const ExprMap &up);
    static bool isLinear(const std::vector<std::vector<Expression>> &gs);
    static bool isLinear(const std::vector<UpdateMap> &up);
    static bool isLinear(const std::vector<ExprMap> &up);
    static bool isPolynomial(const std::vector<Expression> &guard);
    static bool isPolynomial(const UpdateMap &up);
    static bool isPolynomial(const ExprMap &up);
    static bool isPolynomial(const std::vector<std::vector<Expression>> &gs);
    static bool isPolynomial(const std::vector<UpdateMap> &up);
    static bool isPolynomial(const std::vector<ExprMap> &up);
};

#endif // SMT_H
