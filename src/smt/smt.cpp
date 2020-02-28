#include "smt.hpp"
#include "../util/timeout.hpp"
#include "smtfactory.hpp"

Smt::~Smt() {}

void Smt::add(const Expression &e) {
    this->add(buildLit(e));
}

Smt::Result Smt::check(const BoolExpr &e, const VariableManager &varMan) {
    std::unique_ptr<Smt> s = SmtFactory::solver(Smt::chooseLogic({e}), varMan);
    s->add(e);
    return s->check();
}

bool Smt::isImplication(const BoolExpr &lhs, const BoolExpr &rhs, const VariableManager &varMan) {
    std::unique_ptr<Smt> s = SmtFactory::solver(Smt::chooseLogic({lhs, rhs}), varMan);
    s->add(lhs);
    s->add(!rhs);
    return s->check() == Smt::Unsat;
}

option<ExprSymbolMap<GiNaC::numeric>> Smt::maxSmt(BoolExpr hard, std::vector<BoolExpr> soft, uint timeout, const VariableManager &varMan) {
    BoolExpr all = hard & buildAnd(soft);
    std::unique_ptr<Smt> s = SmtFactory::modelBuildingSolver(chooseLogic({all}), varMan);
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

Smt::Logic Smt::chooseLogic(const std::vector<BoolExpr> &xs) {
    Smt::Logic res = Smt::LA;
    for (const BoolExpr &x: xs) {
        if (!x->isLinear()) {
            res = Smt::NA;
        }
        if (!x->isPolynomial()) {
            return Smt::ENA;
        }
    }
    return res;
}

bool Smt::isLinear(const std::vector<Expression> &guard) {
    for (const Expression &e: guard) {
        if (!Expression(e.lhs()).isLinear() || !Expression(e.rhs()).isLinear()) {
            return false;
        }
    }
    return true;
}

bool Smt::isLinear(const UpdateMap &up) {
    for (const auto &p: up) {
        if (!p.second.isLinear()) {
            return false;
        }
    }
    return true;
}

bool Smt::isLinear(const GiNaC::exmap &up) {
    for (const auto &p: up) {
        if (!Expression(p.second).isLinear()) {
            return false;
        }
    }
    return true;
}

bool Smt::isLinear(const std::vector<UpdateMap> &up) {
    for (const UpdateMap &m: up) {
        if (!isLinear(m)) {
            return false;
        }
    }
    return true;
}

bool Smt::isLinear(const std::vector<GiNaC::exmap> &up) {
    for (const GiNaC::exmap &m: up) {
        if (!isLinear(m)) {
            return false;
        }
    }
    return true;
}

bool Smt::isLinear(const std::vector<std::vector<Expression>> &gs) {
    for (const std::vector<Expression> &g: gs) {
        if (!isLinear(g)) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const std::vector<Expression> &guard) {
    for (const Expression &e: guard) {
        if (!Expression(e.lhs()).isPolynomial() || !Expression(e.rhs()).isPolynomial()) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const UpdateMap &up) {
    for (const auto &p: up) {
        if (!p.second.isPolynomial()) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const GiNaC::exmap &up) {
    for (const auto &p: up) {
        if (!Expression(p.second).isPolynomial()) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const std::vector<UpdateMap> &up) {
    for (const UpdateMap &m: up) {
        if (!isPolynomial(m)) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const std::vector<GiNaC::exmap> &up) {
    for (const GiNaC::exmap &m: up) {
        if (!isPolynomial(m)) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const std::vector<std::vector<Expression>> &gs) {
    for (const std::vector<Expression> &g: gs) {
        if (!isPolynomial(g)) {
            return false;
        }
    }
    return true;
}
