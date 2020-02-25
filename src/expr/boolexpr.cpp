#include "boolexpr.hpp"
#include "../expr/relation.hpp"

BoolExpression::~BoolExpression() {}

BoolLit::BoolLit(const Expression &lit): lit(Relation::toLessEq(lit)) { }

bool BoolLit::isAnd() const {
    return false;
}

bool BoolLit::isOr() const {
    return false;
}

option<Expression> BoolLit::getLit() const {
    return {lit};
}

std::set<BoolExpr> BoolLit::getChildren() const {
    return {};
}

const BoolExpr BoolLit::negation() const {
    return std::shared_ptr<BoolExpression>(new BoolLit(Relation::negateLessEqInequality(lit)));
}

BoolLit::~BoolLit() {}


BoolJunction::BoolJunction(const std::set<BoolExpr> &children, ConcatOperator op): children(children), op(op) { }

bool BoolJunction::isAnd() const {
    return op == ConcatAnd;
}

bool BoolJunction::isOr() const {
    return op == ConcatOr;
}

option<Expression> BoolJunction::getLit() const {
    return {};
}

std::set<BoolExpr> BoolJunction::getChildren() const {
    return children;
}

const BoolExpr BoolJunction::negation() const {
    std::set<BoolExpr> newChildren;
    for (const BoolExpr &c: children) {
        newChildren.insert(!c);
    }
    switch (op) {
    case ConcatOr: return buildAnd(newChildren);
    case ConcatAnd: return buildOr(newChildren);
    }
}

BoolJunction::~BoolJunction() {}


BoolExpr build(std::set<BoolExpr> xs, ConcatOperator op) {
    std::stack<BoolExpr> todo;
    for (const BoolExpr &x: xs) {
        todo.push(x);
    }
    std::set<BoolExpr> children;
    while (!todo.empty()) {
        BoolExpr current = todo.top();
        if ((op == ConcatAnd && current->isAnd()) || (op == ConcatOr && current->isOr())) {
            const std::set<BoolExpr> &currentChildren = current->getChildren();
            todo.pop();
            for (const BoolExpr &c: currentChildren) {
                todo.push(c);
            }
        } else {
            children.insert(current);
            todo.pop();
        }
    }
    return std::shared_ptr<BoolExpression>(new BoolJunction(children, op));
}

BoolExpr build(const ExpressionSet &xs, ConcatOperator op) {
    std::set<BoolExpr> children;
    for (const Expression &x: xs) {
        children.insert(buildLit(x));
    }
    return build(children, op);
}

const BoolExpr buildAnd(const ExpressionSet &xs) {
    return build(xs, ConcatAnd);
}

const BoolExpr buildAnd(const std::set<BoolExpr> &xs) {
    return build(xs, ConcatAnd);
}

const BoolExpr buildOr(const ExpressionSet &xs) {
    return build(xs, ConcatOr);
}

const BoolExpr buildOr(const std::set<BoolExpr> &xs) {
    return build(xs, ConcatOr);
}

const BoolExpr buildAnd(const std::vector<Expression> &xs) {
    return build(ExpressionSet(xs.begin(), xs.end()), ConcatAnd);
}

const BoolExpr buildAnd(const std::vector<BoolExpr> &xs) {
    return build(std::set<BoolExpr>(xs.begin(), xs.end()), ConcatAnd);
}

const BoolExpr buildOr(const std::vector<Expression> &xs) {
    return build(ExpressionSet(xs.begin(), xs.end()), ConcatOr);
}

const BoolExpr buildOr(const std::vector<BoolExpr> &xs) {
    return build(std::set<BoolExpr>(xs.begin(), xs.end()), ConcatOr);
}

const BoolExpr buildLit(const Expression &lit) {
    if (Relation::isInequality(lit)) {
        return std::shared_ptr<BoolExpression>(new BoolLit(lit));
    } else {
        assert(Relation::isEquality(lit));
        return buildLit(lit.lhs() <= lit.rhs()) & (lit.lhs() >= lit.rhs());
    }
}

const BoolExpr True = buildAnd(std::vector<BoolExpr>());
const BoolExpr False = buildOr(std::vector<BoolExpr>());

const BoolExpr operator &(const BoolExpr a, const BoolExpr b) {
    const std::set<BoolExpr> &children = {a, b};
    return buildAnd(std::set<BoolExpr>(children));
}

const BoolExpr operator &(const BoolExpr a, const Expression &b) {
    return a & buildLit(b);
}

const BoolExpr operator |(const BoolExpr a, const BoolExpr b) {
    const std::set<BoolExpr> &children = {a, b};
    return buildOr(children);
}

const BoolExpr operator |(const BoolExpr a, const Expression b) {
    return a | buildLit(b);
}

const BoolExpr operator !(const BoolExpr a) {
    return a->negation();
}

bool operator ==(const BoolExpression &a, const BoolExpression &b) {
    if (a.getLit() != b.getLit()) {
        return false;
    }
    if (a.getLit()) {
        return true;
    }
    if (a.isAnd() != b.isAnd()) {
        return false;
    }
    return a.getChildren() == b.getChildren();
}

bool operator <(const BoolExpression &a, const BoolExpression &b) {
    if (a.getLit()) {
        if (!b.getLit()) {
            return true;
        } else {
            return a.getLit().get().compare(b.getLit().get()) < 0;
        }
    }
    if (a.isAnd() && !b.isAnd()) {
        return true;
    }
    if (!a.isAnd() && b.isAnd()) {
        return false;
    }
    return a.getChildren() < b.getChildren();
}
