#include "solver.hpp"

Solver::Solver(const VariableManager &varMan): yicesLa(varMan, smt::LA), yicesNa(varMan, smt::NA), z3(varMan) {
    setTimeout(Config::Z3::DefaultTimeout);
}

Solver::Solver(const VariableManager &varMan, uint timeout): yicesLa(varMan, smt::LA), yicesNa(varMan, smt::NA), z3(varMan) {
    setTimeout(timeout);
}

void Solver::add(const BoolExpr &e) {
    if (logic == smt::LA && !yicesLa.add(e)) {
        logic = smt::NA;
    }
    if (logic <= smt::NA && !yicesNa.add(e)) {
        logic = smt::ENA;
    }
    z3.add(e);
}

void Solver::add(const Rel &e) {
    this->add(buildLit(e));
}

void Solver::push() {
    logicStack.push(logic);
    yicesLa.push();
    yicesNa.push();
    z3.push();
}

void Solver::pop() {
    logic = logicStack.top();
    logicStack.pop();
    yicesLa.pop();
    yicesNa.pop();
    z3.pop();
}

smt::Result Solver::check() {
    switch (logic) {
    case smt::LA: return yicesLa.check();
    case smt::NA: return yicesNa.check();
    case smt::ENA: return z3.check();
    }
    assert(false && "unknown logic");
}

VarMap<GiNaC::numeric> Solver::model() {
    switch (logic) {
    case smt::LA: return yicesLa.model();
    case smt::NA: return yicesNa.model();
    case smt::ENA: return z3.model();
    }
    assert(false && "unknown logic");
}

void Solver::setTimeout(unsigned int timeout) {
    yicesLa.setTimeout(timeout);
    yicesNa.setTimeout(timeout);
    z3.setTimeout(timeout);
}

void Solver::resetSolver() {
    yicesLa.resetSolver();
    yicesNa.resetSolver();
    z3.resetSolver();
}
