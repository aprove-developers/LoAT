#include "smtfactory.hpp"
#include "../config.hpp"
#include "z3/z3.hpp"
#include "yices/yices.hpp"

std::unique_ptr<Smt> SmtFactory::solver(Smt::Logic logic, const VariableManager &varMan, uint timeout) {
    std::unique_ptr<Smt> res;
    switch (logic) {
    case Smt::LA:
        res = std::unique_ptr<Smt>(new Yices(varMan, logic));
        break;
    case Smt::NA:
        res = std::unique_ptr<Smt>(new Z3(varMan));
        break;
    case Smt::ENA:
        res = std::unique_ptr<Smt>(new Z3(varMan));
    }
    res->setTimeout(timeout);
    return res;
}

std::unique_ptr<Smt> SmtFactory::modelBuildingSolver(Smt::Logic logic, const VariableManager &varMan, uint timeout) {
    std::unique_ptr<Smt> res = solver(logic, varMan, timeout);
    res->enableModels();
    return res;
}
