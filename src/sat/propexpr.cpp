#include "propexpr.hpp"

PropExpression::~PropExpression() { }

PropExpr operator&(const PropExpr &a, const PropExpr &b) {
    return PropJunction::buildAnd({a, b});
}

PropExpr operator|(const PropExpr &a, const PropExpr &b) {
    return PropJunction::buildOr({a, b});
}

PropExpr operator!(const PropExpr &a) {
    return a->negate();
}

PropLit::PropLit(int id): id(id) { }

PropExpr PropLit::buildLit(int id) {
    return PropExpr(new PropLit(id));
}

PropExpr PropLit::negate() const {
    return buildLit(-id);
}

option<int> PropLit::lit() const {
    return {id};
}

bool PropLit::isAnd() const {
    return false;
}

bool PropLit::isOr() const {
    return false;
}

PropExprSet PropLit::getChildren() const {
    return {};
}

PropJunction::PropJunction(JunctionType op, const PropExprSet &children): op(op), children(children) { }

PropExpr PropJunction::buildAnd(const PropExprSet &children) {
    return PropExpr(new PropJunction(And, children));
}

PropExpr PropJunction::buildOr(const PropExprSet &children) {
    return PropExpr(new PropJunction(Or, children));
}

PropExpr PropJunction::negate() const {
    PropExprSet newChildren;
    for (const PropExpr &c: children) {
        newChildren.insert(!c);
    }
    return op == And ? buildAnd(newChildren) : buildOr(newChildren);
}

option<int> PropJunction::lit() const {
    return {};
}

bool PropJunction::isAnd() const {
    return op == And;
}

bool PropJunction::isOr() const {
    return op == Or;
}

PropExprSet PropJunction::getChildren() const {
    return children;
}

bool propexpr_compare::operator() (PropExpr a, PropExpr b) const {
    if (a->lit()) {
        if (!b->lit()) {
            return true;
        } else {
            return a->lit().get() < b->lit().get();
        }
    }
    if (a->isAnd() && !b->isAnd()) {
        return true;
    }
    if (!a->isAnd() && b->isAnd()) {
        return false;
    }
    return a->getChildren() < b->getChildren();
}
