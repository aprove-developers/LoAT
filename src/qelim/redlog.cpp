#include "redlog.hpp"
#include "../parser/redlog/redlogparsevisitor.h"
#include <qepcad/qepcad.h>

Redlog::Redlog() {}

RedProc initRedproc() {
    std::string path = std::getenv("PATH");
    std::replace(path.begin(), path.end(), ':', ' ');
    std::vector<std::string> array;
    std::stringstream ss(path);
    std::string temp;
    while (ss >> temp) array.push_back(temp);
    for (const std::string &p: array) {
        std::string redcsl = p + "/redcsl";
        std::ifstream file(redcsl);
        if (file.is_open()) {
            return RedProc_new(redcsl.c_str());
        }
    }
    throw Redlog::RedlogError("couldn't find redcsl binary");
}

RedProc Redlog::process() {
    QEPCADContext Q;
    static RedProc process = initRedproc();
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
