#ifndef METASOLVER_HPP
#define METASOLVER_HPP

#include "../expr/boolexpr.hpp"
#include "smt.hpp"
#include "yices/yices.hpp"
#include "z3/z3.hpp"

class Solver
{
public:
    Solver(const VariableManager &varMan);
    Solver(const VariableManager &varMan, uint timeout);

    void add(const BoolExpr &e);
    void add(const Rel &e);
    void push();
    void pop();
    smt::Result check();
    VarMap<GiNaC::numeric> model();
    void setTimeout(unsigned int timeout);
    void enableModels();
    void resetSolver();

    static smt::Result check(const BoolExpr &e, const VariableManager &varMan) {
        Solver s(varMan);
        s.add(e);
        return s.check();
    }

    static bool isImplication(const BoolExpr &lhs, const BoolExpr &rhs, const VariableManager &varMan) {
        Solver s(varMan);
        s.add(lhs);
        s.add(!rhs);
        return s.check() == smt::Unsat;
    }

private:
    Yices yicesLa;
    Yices yicesNa;
    Z3 z3;

    smt::Logic logic = smt::LA;
    std::stack<smt::Logic> logicStack;

};

#endif // METASOLVER_HPP
