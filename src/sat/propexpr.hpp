#ifndef PROPEXPR_HPP
#define PROPEXPR_HPP

#include <memory>
#include <set>
#include "../util/junctiontype.hpp"
#include "../util/option.hpp"

class PropLit;
class PropJunction;
class PropExpression;
typedef std::shared_ptr<const PropExpression> PropExpr;

struct propexpr_compare {
    bool operator() (PropExpr a, PropExpr b) const;
};

typedef std::set<PropExpr, propexpr_compare> PropExprSet;

class PropExpression: public std::enable_shared_from_this<PropExpression> {

    friend class PropLit;
    friend class PropJunction;

public:

    friend PropExpr operator!(const PropExpr &a);
    virtual ~PropExpression() = 0;
    virtual option<int> lit() const = 0;
    virtual bool isAnd() const = 0;
    virtual bool isOr() const = 0;
    virtual PropExprSet getChildren() const = 0;

protected:

    virtual PropExpr negate() const = 0;

};

class PropLit: public PropExpression {

public:

    static PropExpr buildLit(int id);

    option<int> lit() const override;
    bool isAnd() const override;
    bool isOr() const override;
    PropExprSet getChildren() const override;

protected:

    PropExpr negate() const override;

private:

    PropLit (int id);
    int id;

};

class PropJunction: public PropExpression {

public:

    static PropExpr buildAnd(const PropExprSet &children);
    static PropExpr buildOr(const PropExprSet &children);

    option<int> lit() const override;
    bool isAnd() const override;
    bool isOr() const override;
    PropExprSet getChildren() const override;

protected:

    PropExpr negate() const override;

private:

    PropJunction(JunctionType op, const PropExprSet &children);

    JunctionType op;
    PropExprSet children;

};

PropExpr operator&(const PropExpr &a, const PropExpr &b);
PropExpr operator|(const PropExpr &a, const PropExpr &b);

#endif // PROPEXPR_HPP
