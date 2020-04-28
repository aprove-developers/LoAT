#include "smt.hpp"
#include "smtfactory.hpp"

Smt::~Smt() {}

uint Smt::add(const BoolExpr &e) {
    if (unsatCores) {
        const BoolExpr &m = buildConst(markerCount);
        marker.push_back(m);
        uint idx = marker.size() - 1;
        markerMap[m] = idx;
        ++markerCount;
        _add((!m) | e);
        return idx;
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
    }
}

void Smt::resetSolver() {
    _resetSolver();
    _resetContext();
    marker.clear();
    markerCount = 0;
    markerStack = std::stack<uint>();
    markerMap.clear();
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
