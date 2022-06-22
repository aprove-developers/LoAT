#include "redlog.hpp"

void Redlog::init() {
    process = RedProc_new(REDUCE_BINARY);
    RedAns output = RedAns_new(process, "rlset r;");
    assert(!output->error);
    RedAns_delete(output);
}

void Redlog::exit() {
    RedProc_delete(process);
}

option<BoolExpr> Redlog::qe(const QuantifiedFormula &qf) {
    std::string command = "rlqe(" + qf.toRedlog() + ");";
    RedAns output = RedAns_new(process, command.c_str());
    if (output->error) {
        RedProc_error(process, command.c_str(), output);
        RedAns_delete(output);
        return {};
    } else {
        std::string str = output->result;
        BoolExpr res = BoolExpression::fromRedlog(str);
        RedAns_delete(output);
        return res;
    }
}
