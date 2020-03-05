#include "smt.hpp"
#include "smtfactory.hpp"

Smt::~Smt() {}

void Smt::add(const Rel &e) {
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

option<VarMap<GiNaC::numeric>> Smt::maxSmt(BoolExpr hard, std::vector<BoolExpr> soft, uint timeout, const VariableManager &varMan) {
    BoolExpr all = hard & buildAnd(soft);
    std::unique_ptr<Smt> s = SmtFactory::modelBuildingSolver(chooseLogic({all}), varMan);
    s->setTimeout(timeout);
    s->add(hard);
    if (s->check() != Sat) {
        return {};
    }
    VarMap<GiNaC::numeric> model = s->model();
    for (const BoolExpr &e: soft) {
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

bool Smt::isLinear(const std::vector<Rel> &guard) {
    for (const Rel &rel: guard) {
        if (!rel.isLinear()) {
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

bool Smt::isLinear(const ExprMap &up) {
    for (const auto &p: up) {
        if (!p.second.isLinear()) {
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

bool Smt::isLinear(const std::vector<ExprMap> &up) {
    for (const ExprMap &m: up) {
        if (!isLinear(m)) {
            return false;
        }
    }
    return true;
}

bool Smt::isLinear(const std::vector<std::vector<Rel>> &gs) {
    for (const std::vector<Rel> &g: gs) {
        if (!isLinear(g)) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const std::vector<Rel> &guard) {
    for (const Rel &rel: guard) {
        if (!rel.isPoly()) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const UpdateMap &up) {
    for (const auto &p: up) {
        if (!p.second.isPoly()) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const ExprMap &up) {
    for (const auto &p: up) {
        if (!p.second.isPoly()) {
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

bool Smt::isPolynomial(const std::vector<ExprMap> &up) {
    for (const ExprMap &m: up) {
        if (!isPolynomial(m)) {
            return false;
        }
    }
    return true;
}

bool Smt::isPolynomial(const std::vector<std::vector<Rel>> &gs) {
    for (const std::vector<Rel> &g: gs) {
        if (!isPolynomial(g)) {
            return false;
        }
    }
    return true;
}
