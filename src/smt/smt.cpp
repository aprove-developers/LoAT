#include "smt.hpp"
#include "../util/timeout.hpp"
#include "smtfactory.hpp"

Smt::~Smt() {}

void Smt::add(const Expression &e) {
    this->add(buildLit(e));
}

Smt::Result Smt::check(const BoolExpr &e) {
    std::unique_ptr<Smt> s = SmtFactory::solver();
    s->add(e);
    return s->check();
}

bool Smt::isImplication(const BoolExpr &lhs, const BoolExpr &rhs) {
    std::unique_ptr<Smt> s = SmtFactory::solver();
    s->add(lhs);
    s->add(!rhs);
    return s->check() == Smt::Unsat;
}

option<ExprSymbolMap<GiNaC::numeric>> Smt::maxSmt(BoolExpr hard, std::vector<BoolExpr> soft, uint timeout) {
    std::unique_ptr<Smt> s = SmtFactory::solver();
    s->setTimeout(timeout);
    s->add(hard);
    if (s->check() != Sat) {
        return {};
    }
    ExprSymbolMap<GiNaC::numeric> model = s->model();
    for (const BoolExpr &e: soft) {
        if (Timeout::soft()) {
            return {};
        }
        s->push();
        s->add(e);
        if (s->check() == Sat) {
            model = s->model();
        } else {
            s->pop();
        }
    }
    return {model};
}
