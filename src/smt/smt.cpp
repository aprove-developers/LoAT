#include "smt.hpp"
#include "smtfactory.hpp"

Smt::~Smt() {}

uint Smt::add(const Rel &e) {
    return this->add(buildLit(e));
}

SatResult Smt::check(const BoolExpr &e, const VariableManager &varMan) {
    std::unique_ptr<Smt> s = SmtFactory::solver(Smt::chooseLogic({e}), varMan);
    s->add(e);
    return s->check();
}

bool Smt::isImplication(const BoolExpr &lhs, const BoolExpr &rhs, const VariableManager &varMan) {
    std::unique_ptr<Smt> s = SmtFactory::solver(Smt::chooseLogic({lhs, rhs}), varMan);
    s->add(lhs);
    s->add(!rhs);
    return s->check() == Unsat;
}

Smt::Logic Smt::chooseLogic(const std::vector<BoolExpr> &xs, const std::vector<Subs> &up) {
    Smt::Logic res = Smt::LA;
    for (const BoolExpr &x: xs) {
        if (!(x->isLinear())) {
            if (!(x->isPolynomial())) {
                return Smt::ENA;
            }
            res = Smt::NA;
        }
    }
    for (const Subs &u: up) {
        if (!u.isLinear()) {
            if (!u.isPoly()) {
                return Smt::ENA;
            }
            res = Smt::NA;
        }
    }
    return res;
}
