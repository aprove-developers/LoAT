#ifndef BOOLEXPR_HPP
#define BOOLEXPR_HPP

#include "../its/types.hpp"

class BoolLit;
class BoolJunction;
class BoolExpression;
typedef std::shared_ptr<BoolExpression> BoolExpr;

class BoolExpression {
public:
    virtual option<Rel> getLit() const = 0;
    virtual bool isAnd() const = 0;
    virtual bool isOr() const = 0;
    virtual std::set<BoolExpr> getChildren() const = 0;
    virtual const BoolExpr negation() const = 0;
    virtual bool isLinear() const = 0;
    virtual bool isPolynomial() const = 0;
    virtual ~BoolExpression();
};

class BoolLit: public BoolExpression {

private:

    Rel lit;

public:

    BoolLit(const Rel &lit);
    bool isAnd() const override;
    bool isOr() const override;
    option<Rel> getLit() const override;
    std::set<BoolExpr> getChildren() const override;
    const BoolExpr negation() const override;
    bool isLinear() const override;
    bool isPolynomial() const override;
    ~BoolLit() override;

};

enum ConcatOperator { ConcatAnd, ConcatOr };

class BoolJunction: public BoolExpression {

private:

    std::set<BoolExpr> children;
    ConcatOperator op;

public:

    BoolJunction(const std::set<BoolExpr> &children, ConcatOperator op);
    bool isAnd() const override;
    bool isOr() const override;
    option<Rel> getLit() const override;
    std::set<BoolExpr> getChildren() const override;
    const BoolExpr negation() const override;
    bool isLinear() const override;
    bool isPolynomial() const override;
    ~BoolJunction() override;

};

const BoolExpr buildAnd(const std::vector<Rel> &xs);
const BoolExpr buildAnd(const std::vector<BoolExpr> &xs);
const BoolExpr buildOr(const std::vector<Rel> &xs);
const BoolExpr buildOr(const std::vector<BoolExpr> &xs);
const BoolExpr buildAnd(const RelationSet &xs);
const BoolExpr buildAnd(const std::set<BoolExpr> &xs);
const BoolExpr buildOr(const RelationSet &xs);
const BoolExpr buildOr(const std::set<BoolExpr> &xs);
const BoolExpr buildLit(const Rel &lit);

extern const BoolExpr True;
extern const BoolExpr False;

const BoolExpr operator &(const BoolExpr a, const BoolExpr b);
const BoolExpr operator &(const BoolExpr a, const Rel &b);
const BoolExpr operator |(const BoolExpr a, const BoolExpr b);
const BoolExpr operator |(const BoolExpr a, const Rel b);
const BoolExpr operator !(const BoolExpr);

bool operator ==(const BoolExpression &a, const BoolExpression &b);
bool operator <(const BoolExpression &a, const BoolExpression &b);
std::ostream& operator<<(std::ostream &s, const BoolExpr &e);

#endif // BOOLEXPR_HPP
