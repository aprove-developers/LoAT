#ifndef SMT_H
#define SMT_H

#include "../its/types.hpp"
#include "../expr/boolexpr.hpp"

class Smt
{
public:

    enum Result {Sat, Unknown, Unsat};

    virtual void add(const BoolExpr &e) = 0;
    void add(const Expression &e);
    virtual void push() = 0;
    virtual void pop() = 0;
    virtual Result check() = 0;
    virtual ExprSymbolMap<GiNaC::numeric> model() = 0;
    virtual void setTimeout(unsigned int timeout) = 0;
    virtual void resetSolver() = 0;
    virtual std::ostream& print(std::ostream& os) const = 0;
    virtual ~Smt();

    static option<ExprSymbolMap<GiNaC::numeric>> maxSmt(BoolExpr hard, std::vector<BoolExpr> soft);
    static Smt::Result check(const BoolExpr &e);
    static bool isImplication(const BoolExpr &lhs, const BoolExpr &rhs);

};

#endif // SMT_H
