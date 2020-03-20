#ifndef SMT_H
#define SMT_H

#include "../expr/expression.hpp"
#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"

class Smt
{
public:

    enum Result {Sat, Unknown, Unsat};
    enum Logic {LA, NA, ENA};

    virtual void add(const BoolExpr &e) = 0;
    void add(const Rel &e);
    virtual void push() = 0;
    virtual void pop() = 0;
    virtual Result check() = 0;
    virtual VarMap<GiNaC::numeric> model() = 0;
    virtual void setTimeout(unsigned int timeout) = 0;
    virtual void enableModels() = 0;
    virtual void resetSolver() = 0;
    virtual ~Smt();

    static Smt::Result check(const BoolExpr &e, const VariableManager &varMan);
    static bool isImplication(const BoolExpr &lhs, const BoolExpr &rhs, const VariableManager &varMan);
    static Logic chooseLogic(const std::vector<BoolExpr> &xs);

    template<class T> static Logic chooseLogic(const std::vector<std::vector<Rel>> &g, const std::vector<T> &up) {
        if (isLinear(g) && isLinear(up)) {
            return Smt::LA;
        }
        if (isPolynomial(g) && isPolynomial(up)) {
            return Smt::NA;
        }
        return Smt::ENA;
    }

private:
    static bool isLinear(const std::vector<Rel> &guard);
    static bool isLinear(const Subs &up);
    static bool isLinear(const std::vector<std::vector<Rel>> &gs);
    static bool isLinear(const std::vector<Subs> &up);
    static bool isPolynomial(const std::vector<Rel> &guard);
    static bool isPolynomial(const Subs &up);
    static bool isPolynomial(const std::vector<std::vector<Rel>> &gs);
    static bool isPolynomial(const std::vector<Subs> &up);
};

#endif // SMT_H
