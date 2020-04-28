#include "smt.hpp"
#include "smtfactory.hpp"

Smt::~Smt() {}

uint Smt::add(const ForAllExpr &e) {
    if (unsatCores) {
        const BoolExpr &m = buildConst(markerCount);
        marker.push_back(m);
        uint idx = marker.size() - 1;
        markerMap[m] = idx;
        ++markerCount;
        ForAllExpr imp = ((!m) | e.expr)->quantify(e.boundVars);
        _add(imp);
        return idx;
    } else {
        _add(e);
        return 0;
    }
}

uint Smt::add(const BoolExpr &e) {
    return add(e->quantify({}));
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

Smt::Logic Smt::chooseLogic(const std::vector<ForAllExpr> &xs, const std::vector<Subs> &up) {
    std::vector<BoolExpr> boolExpr;
    bool quantified = false;
    for (const ForAllExpr &x: xs) {
        boolExpr.push_back(x.expr);
        quantified |= !x.boundVars.empty();
    }
    Logic l = chooseLogic(boolExpr, up);
    if (!quantified) return l;
    switch (l) {
    case Smt::QF_LA: return LA;
    case Smt::QF_NA: return NA;
    case Smt::QF_ENA: return ENA;
    default: assert(false && "chooseLogic returned quantified logic for quantifier free expressions");
    }
}
