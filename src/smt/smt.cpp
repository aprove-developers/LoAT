#include "smt.hpp"
#include "smtfactory.hpp"

Smt::Smt(VariableManager &varMan): varMan(varMan) {}

Smt::~Smt() {}

uint Smt::add(const BoolExpr &e) {
    if (unsatCores) {
        const BoolExpr &m = varMan.freshBoolVar();
        marker.push_back(m);
        _add((!m) | e);
        return marker.size() - 1;
    } else {
        _add(e);
        return 0;
    }
}

void Smt::push() {
    _push();
    if (unsatCores) {
        markerStack.push(marker.size());
    }
}

void Smt::pop() {
    _pop();
    if (unsatCores) {
        marker.resize(markerStack.top());
        markerStack.pop();
    }
}

void Smt::resetSolver() {
    _resetSolver();
    _resetContext();
    marker.clear();
    markerStack = std::stack<uint>();
    updateParams();
}

void Smt::enableModels() {
    this->models = true;
    updateParams();
}

void Smt::enableUnsatCores() {
    this->unsatCores = true;
    updateParams();
}

void Smt::setTimeout(unsigned int timeout) {
    this->timeout = timeout;
    updateParams();
}

uint Smt::add(const Rel &e) {
    return this->add(buildLit(e));
}

Smt::Result Smt::check(const BoolExpr &e, VariableManager &varMan) {
    std::unique_ptr<Smt> s = SmtFactory::solver(Smt::chooseLogic({e}), varMan);
    s->add(e);
    return s->check();
}

bool Smt::isImplication(const BoolExpr &lhs, const BoolExpr &rhs, VariableManager &varMan) {
    std::unique_ptr<Smt> s = SmtFactory::solver(Smt::chooseLogic({lhs, rhs}), varMan);
    s->add(lhs);
    s->add(!rhs);
    return s->check() == Smt::Unsat;
}

Smt::Logic Smt::chooseLogic(const std::vector<BoolExpr> &xs, const std::vector<Subs> &up) {
    Smt::Logic res = Smt::QF_LA;
    for (const BoolExpr &x: xs) {
        if (!(x->isLinear())) {
            if (!(x->isPolynomial())) {
                return Smt::QF_ENA;
            }
            res = Smt::QF_NA;
        }
    }
    for (const Subs &u: up) {
        if (!u.isLinear()) {
            if (!u.isPoly()) {
                return Smt::QF_ENA;
            }
            res = Smt::QF_NA;
        }
    }
    return res;
}
