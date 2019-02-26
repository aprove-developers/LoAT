//
// Created by ffrohn on 2/14/19.
//

#include "z3solver.h"

using namespace std;

option<z3::model> Z3Solver::maxSmt(std::vector<z3::expr> hard, std::vector<z3::expr> soft) {
    for (const z3::expr &e: hard) {
        this->add(e);
    }
    if (this->check() != z3::check_result::sat) {
        return {};
    }
    z3::model model = this->get_model();
    for (const z3::expr &e: soft) {
        this->push();
        this->add(e);
        if (this->check() == z3::check_result::sat) {
            model = this->get_model();
        } else {
            this->pop();
        }
    }
    return model;
}
