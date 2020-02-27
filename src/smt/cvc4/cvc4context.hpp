#ifdef HAS_CVC4

#ifndef CVC4CONTEXT_HPP
#define CVC4CONTEXT_HPP

#include <cvc4/cvc4.h>
#include "../smtcontext.hpp"
#include "../../its/types.hpp"

class Cvc4Context: public CVC4::ExprManager, public SmtContext<CVC4::Expr>
{

public:

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

protected:

    CVC4::Expr buildVar(const std::string &basename, Expression::Type type) override;

};

#endif // CVC4CONTEXT_HPP

#endif
