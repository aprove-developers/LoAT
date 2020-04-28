#ifndef CVC4CONTEXT_HPP
#define CVC4CONTEXT_HPP

#include <cvc4/cvc4.h>
#include "../smtcontext.hpp"
#include "../../its/types.hpp"

class Cvc4Context: public SmtContext<CVC4::Expr>
{

public:

    Cvc4Context(CVC4::ExprManager &manager);
    ~Cvc4Context() override;
    CVC4::Expr getInt(long val) override;
    CVC4::Expr getReal(long num, long denom) override;
    CVC4::Expr pow(const CVC4::Expr &base, const CVC4::Expr &exp) override;
    CVC4::Expr plus(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr times(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr eq(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr lt(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr le(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr gt(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr ge(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr neq(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr bAnd(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr bOr(const CVC4::Expr &x, const CVC4::Expr &y) override;
    CVC4::Expr bTrue() const override;
    CVC4::Expr bFalse() const override;
    CVC4::Expr negate(const CVC4::Expr &x) override;

    bool isLit(const CVC4::Expr &e) const override;
    bool isTrue(const CVC4::Expr &e) const override;
    bool isFalse(const CVC4::Expr &e) const override;
    bool isNot(const CVC4::Expr &e) const override;
    std::vector<CVC4::Expr> getChildren(const CVC4::Expr &e) const override;
    bool isAnd(const CVC4::Expr &e) const override;
    bool isAdd(const CVC4::Expr &e) const override;
    bool isMul(const CVC4::Expr &e) const override;
    bool isDiv(const CVC4::Expr &e) const override;
    bool isPow(const CVC4::Expr &e) const override;
    bool isVar(const CVC4::Expr &e) const override;
    bool isRationalConstant(const CVC4::Expr &e) const override;
    bool isInt(const CVC4::Expr &e) const override;
    long toInt(const CVC4::Expr &e) const override;
    long numerator(const CVC4::Expr &e) const override;
    long denominator(const CVC4::Expr &e) const override;
    CVC4::Expr lhs(const CVC4::Expr &e) const override;
    CVC4::Expr rhs(const CVC4::Expr &e) const override;
    Rel::RelOp relOp(const CVC4::Expr &e) const override;
    std::string getName(const CVC4::Expr &e) const override;

    void printStderr(const CVC4::Expr &e) const override;

private:
    CVC4::ExprManager &manager;
    CVC4::Expr buildVar(const std::string &basename, Expr::Type type) override;
    CVC4::Expr buildBoundVar(const std::string &basename, Expr::Type type) override;
    CVC4::Expr buildConst(uint id) override;
    std::map<CVC4::Expr, std::string> varNames;

};

#endif // CVC4CONTEXT_HPP
