#include "redlog.hpp"
#include "../parser/redlog/redlogparsevisitor.h"

Redlog::Redlog() {}

RedProc Redlog::process() {
    static RedProc process = RedProc_new(REDUCE_BINARY);
    return process;
}

void Redlog::init() {
    const char* cmd = "rlset r;";
    RedAns output = RedAns_new(process(), cmd);
    if (output->error) {
        RedProc_error(process(), cmd, output);
    }
    RedAns_delete(output);
}

void Redlog::exit() {
    RedProc_delete(process());
}

option<BoolExpr> Redlog::qe(const QuantifiedFormula &qf, VariableManager &varMan) {
    std::string command = "rlqe(" + qf.toRedlog() + ");";
//    std::cout << "command: " << command << std::endl;
    RedAns output = RedAns_new(process(), command.c_str());
    if (output->error) {
        RedProc_error(process(), command.c_str(), output);
        RedAns_delete(output);
    } else {
        std::string str = output->result;
//        std::cout << "redlog: " << output->result << std::endl;
        BoolExpr res = RedlogParseVisitor::parse(str, varMan);
        RedAns_delete(output);
        return res;
    }
    return {};
}
