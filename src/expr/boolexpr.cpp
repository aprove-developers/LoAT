#include "boolexpr.hpp"
#include <vector>
#include <functional>
#include <iostream>
#include <algorithm>

BoolExpression::~BoolExpression() {}

RelSet BoolExpression::lits() const {
    RelSet res;
    collectLits(res);
    return res;
}

Guard BoolExpression::conjunctionToGuard() const {
    const RelSet &lits = this->lits();
    return Guard(lits.begin(), lits.end());
}

VarSet BoolExpression::vars() const {
    VarSet res;
    collectVars(res);
    return res;
}

std::vector<Guard> BoolExpression::dnf() const {
    std::vector<Guard> res;
    dnf(res);
    return res;
}

QuantifiedFormula BoolExpression::quantify(const std::vector<Quantifier> &prefix) const {
    return QuantifiedFormula(prefix, shared_from_this());
}

BoolLit::BoolLit(const Rel &lit): lit(lit.makeRhsZero()) { }

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
    return BoolExpr(new BoolLit(!lit));
}

bool BoolLit::isLinear() const {
    return lit.isLinear();
}

bool BoolLit::isPolynomial() const {
    return lit.isPoly();
}

BoolExpr BoolLit::subs(const Subs &subs) const {
    return buildLit(lit.subs(subs));
}

BoolExpr BoolLit::toG() const {
    if (lit.isEq()) {
        std::vector<Rel> lits = {lit.lhs() - lit.rhs() >= 0, lit.rhs() - lit.lhs() >= 0};
        return buildAnd(lits);
    } else if (lit.isNeq()) {
        std::vector<Rel> lits = {lit.lhs() - lit.rhs() > 0, lit.rhs() - lit.lhs() > 0};
        return buildOr(lits);
    } else if (lit.isGZeroConstraint()) {
        return shared_from_this();
    } else {
        return buildLit(lit.makeRhsZero().toG());
    }
}

BoolExpr BoolLit::toLeq() const {
    if (lit.isIneq()) {
        if (lit.relOp() == Rel::leq) {
            return shared_from_this();
        } else {
            return buildLit(lit.toLeq());
        }
    } else if (lit.isEq()) {
        std::vector<Rel> rels {lit.lhs() <= lit.rhs(), lit.rhs() <= lit.lhs()};
        return buildAnd(rels);
    } else {
        assert(lit.isNeq());
        std::vector<Rel> rels {(lit.lhs() < lit.rhs()).toLeq(), (lit.rhs() < lit.lhs()).toLeq()};
        return buildOr(rels);
    }
}

bool BoolLit::isConjunction() const {
    return true;
}

size_t BoolLit::size() const {
    return 1;
}

std::string BoolLit::toRedlog() const {
    return lit.toString();
}

void BoolLit::collectLits(RelSet &res) const {
    res.insert(lit);
}

void BoolLit::collectVars(VarSet &res) const {
    const VarSet &litVars = lit.vars();
    res.insert(litVars.begin(), litVars.end());
}

BoolExpr BoolLit::replaceRels(const RelMap<BoolExpr> map) const {
    if (map.count(lit) > 0) {
        return map.at(lit);
    } else {
        return shared_from_this();
    }
}

void BoolLit::dnf(std::vector<Guard> &res) const {
    if (res.empty()) {
        res.push_back({lit});
    } else {
        for (Guard &g: res) {
            g.push_back(lit);
        }
    }
}

unsigned BoolLit::hash() const {
    return lit.hash();
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
    throw std::invalid_argument("unknown junction");
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

BoolExpr BoolJunction::subs(const Subs &subs) const {
    BoolExprSet newChildren;
    for (const BoolExpr &c: children) {
        newChildren.insert(c->subs(subs));
    }
    return isAnd() ? buildAnd(newChildren) : buildOr(newChildren);
}

BoolExpr BoolJunction::toG() const {
    BoolExprSet newChildren;
    for (const BoolExpr &c: children) {
        newChildren.insert(c->toG());
    }
    return isAnd() ? buildAnd(newChildren) : buildOr(newChildren);
}

BoolExpr BoolJunction::toLeq() const {
    BoolExprSet newChildren;
    for (const BoolExpr &c: children) {
        newChildren.insert(c->toLeq());
    }
    return isAnd() ? buildAnd(newChildren) : buildOr(newChildren);
}

bool BoolJunction::isConjunction() const {
    return isAnd() && std::all_of(children.begin(), children.end(), [](const BoolExpr c){
        return c->isConjunction();
    });
}

size_t BoolJunction::size() const {
    size_t res = 1;
    for (const BoolExpr &c: children) {
        res += c->size();
    }
    return res;
}

std::string BoolJunction::toRedlog() const {
    std::string infix = isAnd() ? " and " : " or ";
    std::string res;
    bool first = true;
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (first) first = false;
        else res += infix;
        res += (*it)->toRedlog();
    }
    return res;
}

void BoolJunction::collectLits(RelSet &res) const {
    for (const BoolExpr &c: children) {
        c->collectLits(res);
    }
}

void BoolJunction::collectVars(VarSet &res) const {
    for (const BoolExpr &c: children) {
        c->collectVars(res);
    }
}

BoolExpr BoolJunction::replaceRels(const RelMap<BoolExpr> map) const {
    BoolExprSet newChildren;
    for (const BoolExpr &c: children) {
        const option<BoolExpr> &newC = c->replaceRels(map);
        if (newC) {
            newChildren.insert(newC.get());
        }
    }
    return isAnd() ? buildAnd(newChildren) : buildOr(newChildren);
}

void BoolJunction::dnf(std::vector<Guard> &res) const {
    if (isAnd()) {
        for (const BoolExpr &e: children) {
            e->dnf(res);
        }
    } else {
        std::vector<Guard> oldRes(res);
        res.clear();
        for (const BoolExpr &e: children) {
            std::vector<Guard> newRes(oldRes);
            e->dnf(newRes);
            res.insert(res.end(), newRes.begin(), newRes.end());
        }
    }
}

unsigned BoolJunction::hash() const {
    unsigned hash = 7;
    for (const BoolExpr& c: children) {
        hash = 31 * hash + c->hash();
    }
    hash = 31 * hash + op;
    return hash;
}

BoolJunction::~BoolJunction() {}


Quantifier::Quantifier(const Type &qType, const VarSet &vars): qType(qType), vars(vars) {}

Quantifier Quantifier::negation() const {
    auto _qType = qType == Type::Exists ? Type::Forall : Type::Exists;
    return Quantifier(_qType, vars);
}

const VarSet& Quantifier::getVars() const {
    return vars;
}

Quantifier::Type Quantifier::getType() const {
    return qType;
}

std::string Quantifier::toRedlog() const {
    std::string q = qType == Type::Exists ? "ex" : "all";
    std::string res;
    for (const auto& var: vars) {
        res = q + "(" + var.get_name();
    }
    return res;
}

QuantifiedFormula::QuantifiedFormula(std::vector<Quantifier> prefix, const BoolExpr &matrix): prefix(prefix), matrix(matrix) {}

const QuantifiedFormula QuantifiedFormula::negation() const {
    std::vector<Quantifier> _prefix;
    std::transform(prefix.begin(), prefix.end(), _prefix.begin(), [](const auto &q ){return q.negation();});
    return QuantifiedFormula(_prefix, matrix->negation());
}

bool QuantifiedFormula::isLinear() const {
    return matrix->isLinear();
}

bool QuantifiedFormula::isPolynomial() const {
    return matrix->isPolynomial();
}

VarSet QuantifiedFormula::boundVars() const {
    VarSet res;
    for (const Quantifier &q: prefix) {
        res.insert(q.getVars().begin(), q.getVars().end());
    }
    return res;
}

QuantifiedFormula QuantifiedFormula::subs(const Subs &subs) const {
    auto dom = subs.domain();
    Subs projected = subs.project(freeVars());
    return QuantifiedFormula(prefix, matrix->subs(projected));
}

QuantifiedFormula QuantifiedFormula::toG() const {
    return QuantifiedFormula(prefix, matrix->toG());
}

QuantifiedFormula QuantifiedFormula::toLeq() const {
    return QuantifiedFormula(prefix, matrix->toLeq());
}

void QuantifiedFormula::collectLits(RelSet &res) const {
    matrix->collectLits(res);
}

VarSet QuantifiedFormula::freeVars() const {
    VarSet vars, res;
    VarSet bv = boundVars();
    matrix->collectVars(vars);
    std::set_difference(vars.begin(), vars.end(), bv.begin(), bv.end(), std::inserter(res, res.begin()));
    return res;
}

std::string QuantifiedFormula::toRedlog() const {
    std::string res;
    for (const auto &q: prefix) {
        res += q.toRedlog();
    }
    res += matrix->toRedlog();
    for (const auto &q: prefix) {
        unsigned size = q.getVars().size();
        for (unsigned i = 0; i < size; ++i) {
            res += ")";
        }
    }
    return res;
}

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
    return BoolExpr(new BoolJunction(children, op));
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

const BoolExpr buildConjunctiveClause(const BoolExprSet &xs) {
    return BoolExpr(new BoolJunction(xs, ConcatAnd));
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
    return BoolExpr(new BoolLit(lit));
}

const BoolExpr True = buildAnd(std::vector<BoolExpr>());
const BoolExpr False = buildOr(std::vector<BoolExpr>());

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

bool operator ==(const BoolExpr a, const BoolExpr b) {
    if (a->getLit() != b->getLit()) {
        return false;
    }
    if (a->getLit()) {
        return true;
    }
    if (a->isAnd() != b->isAnd()) {
        return false;
    }
    return a->getChildren() == b->getChildren();
}

bool operator !=(const BoolExpr a, const BoolExpr b) {
    return !(a==b);
}

bool boolexpr_compare::operator() (BoolExpr a, BoolExpr b) const {
    if (a->getLit()) {
        if (!b->getLit()) {
            return true;
        } else {
            return a->getLit().get() < b->getLit().get();
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
