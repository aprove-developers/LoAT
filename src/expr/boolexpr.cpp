#include "boolexpr.hpp"

BoolExpression::~BoolExpression() {}

BoolLit::BoolLit(const Rel &lit): lit(lit.toLeq()) { }

bool BoolLit::isAnd() const {
    return false;
}

bool BoolLit::isOr() const {
    return false;
}

option<Rel> BoolLit::getLit() const {
    return {lit};
}

BoolExprSet BoolLit::getChildren() const {
    return {};
}

const BoolExpr BoolLit::negation() const {
    return std::shared_ptr<BoolExpression>(new BoolLit(!lit));
}

bool BoolLit::isLinear() const {
    return lit.isLinear();
}

bool BoolLit::isPolynomial() const {
    return lit.isPoly();
}

BoolLit::~BoolLit() {}


BoolJunction::BoolJunction(const BoolExprSet &children, ConcatOperator op): children(children), op(op) { }

bool BoolJunction::isAnd() const {
    return op == ConcatAnd;
}

bool BoolJunction::isOr() const {
    return op == ConcatOr;
}

option<Rel> BoolJunction::getLit() const {
    return {};
}

BoolExprSet BoolJunction::getChildren() const {
    return children;
}

const BoolExpr BoolJunction::negation() const {
    BoolExprSet newChildren;
    for (const BoolExpr &c: children) {
        newChildren.insert(!c);
    }
    switch (op) {
    case ConcatOr: return buildAnd(newChildren);
    case ConcatAnd: return buildOr(newChildren);
    }
    assert(false && "unknown junction");
}

bool BoolJunction::isLinear() const {
    for (const BoolExpr &e: children) {
        if (!e->isLinear()) {
            return false;
        }
    }
    return true;
}

bool BoolJunction::isPolynomial() const {
    for (const BoolExpr &e: children) {
        if (!e->isPolynomial()) {
            return false;
        }
    }
    return true;
}


BoolJunction::~BoolJunction() {}


BoolExpr build(BoolExprSet xs, ConcatOperator op) {
    std::stack<BoolExpr> todo;
    for (const BoolExpr &x: xs) {
        todo.push(x);
    }
    BoolExprSet children;
    while (!todo.empty()) {
        BoolExpr current = todo.top();
        if ((op == ConcatAnd && current->isAnd()) || (op == ConcatOr && current->isOr())) {
            const BoolExprSet &currentChildren = current->getChildren();
            todo.pop();
            for (const BoolExpr &c: currentChildren) {
                todo.push(c);
            }
        } else {
            children.insert(current);
            todo.pop();
        }
    }
    if (children.size() == 1) {
        return *children.begin();
    }
    return std::shared_ptr<BoolExpression>(new BoolJunction(children, op));
}

BoolExpr build(const RelSet &xs, ConcatOperator op) {
    BoolExprSet children;
    for (const Rel &x: xs) {
        children.insert(buildLit(x));
    }
    return build(children, op);
}

const BoolExpr buildAnd(const RelSet &xs) {
    return build(xs, ConcatAnd);
}

const BoolExpr buildAnd(const BoolExprSet &xs) {
    return build(xs, ConcatAnd);
}

const BoolExpr buildOr(const RelSet &xs) {
    return build(xs, ConcatOr);
}

const BoolExpr buildOr(const BoolExprSet &xs) {
    return build(xs, ConcatOr);
}

const BoolExpr buildAnd(const std::vector<Rel> &xs) {
    return build(RelSet(xs.begin(), xs.end()), ConcatAnd);
}

const BoolExpr buildAnd(const std::vector<BoolExpr> &xs) {
    return build(BoolExprSet(xs.begin(), xs.end()), ConcatAnd);
}

const BoolExpr buildOr(const std::vector<Rel> &xs) {
    return build(RelSet(xs.begin(), xs.end()), ConcatOr);
}

const BoolExpr buildOr(const std::vector<BoolExpr> &xs) {
    return build(BoolExprSet(xs.begin(), xs.end()), ConcatOr);
}

const BoolExpr buildLit(const Rel &lit) {
    if (lit.isIneq()) {
        return std::shared_ptr<BoolExpression>(new BoolLit(lit));
    } else if (lit.isEq()) {
        return buildLit(lit.lhs() <= lit.rhs()) & (lit.lhs() >= lit.rhs());
    } else {
        assert(lit.isNeq());
        return buildLit(lit.lhs() < lit.rhs()) | (lit.lhs() > lit.rhs());
    }
}

const BoolExpr True = buildAnd(std::vector<BoolExpr>());
const BoolExpr False = buildOr(std::vector<BoolExpr>());

bool BoolExpr_is_less::operator()(const BoolExpr a, const BoolExpr b) const {
    return *a < *b;
}

const BoolExpr operator &(const BoolExpr a, const BoolExpr b) {
    const BoolExprSet &children = {a, b};
    return buildAnd(children);
}

const BoolExpr operator &(const BoolExpr a, const Rel &b) {
    return a & buildLit(b);
}

const BoolExpr operator |(const BoolExpr a, const BoolExpr b) {
    const BoolExprSet &children = {a, b};
    return buildOr(children);
}

const BoolExpr operator |(const BoolExpr a, const Rel b) {
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
            return a.getLit().get() < b.getLit().get();
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

std::ostream& operator<<(std::ostream &s, const BoolExpr e) {
    if (e->getLit()) {
        s << e->getLit().get();
    } else if (e->getChildren().empty()) {
        if (e->isAnd()) {
            s << "TRUE";
        } else {
            s << "FALSE";
        }
    } else {
        bool first = true;
        s << "(";
        for (const BoolExpr &c: e->getChildren()) {
            if (first) {
                s << c;
                first = false;
            } else {
                if (e->isAnd()) {
                    s << " /\\ ";
                } else {
                    s << " \\/ ";
                }
                s << c;
            }
        }
        s << ")";
    }
    return s;
}
