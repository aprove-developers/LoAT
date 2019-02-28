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
        if (Timeout::soft()) {
            return {};
        }
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

void Z3Solver::setTimeout(Z3Context &context, unsigned int timeout) {
    if (this->timeout && this->timeout.get() == timeout) {
        return;
    } else if (timeout > 0) {
        z3::params params(context);
        params.set(":timeout", timeout);
        this->set(params);
        this->timeout = {timeout};
    }
}
