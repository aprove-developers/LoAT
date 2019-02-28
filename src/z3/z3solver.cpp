//
// Created by ffrohn on 2/14/19.
//

#include "z3solver.h"

using namespace std;

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
