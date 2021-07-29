#include "cintegerexport.hpp"
#include "../expr/boolexpr.hpp"
#include <boost/algorithm/string.hpp>

namespace c_integer_export {

void doExport(const ITSProblem& its) {
    std::stringstream res;
    res << "typedef enum {false, true} bool;\n";
    res << "extern int __VERIFIER_nondet_int(void);\n";
    res << "int main() {\n";
    for (const auto &x: its.getVars()) {
        if (!its.isTempVar(x)) throw std::invalid_argument("");
        res << "    int " << x << ";\n";
    }
    for (const auto &x: its.getVars()) {
        res << "    " << x << " = __VERIFIER_nondet_int();\n";
    }
    bool found_loop = false;
    bool found_init = false;
    for (auto idx: its.getAllTransitions()) {
        const auto& rule = its.getRule(idx);
        if (rule.getLhsLoc() == its.getInitialLocation()) {
            if (rule.getGuard() != True) throw std::invalid_argument("");
            for (const auto& p: rule.getUpdate(0)) {
                if (!p.second.equals(p.first)) throw std::invalid_argument("");
            }
            if (found_init) throw std::invalid_argument("");
            found_init = true;
        } else {
            if (rule.isSimpleLoop()) throw std::invalid_argument("");
            if (!found_loop) throw std::invalid_argument("");
            found_loop = true;
            std::stringstream cond;
            cond << rule.getGuard();
            std::string cond_str = cond.str();
            boost::replace_all(cond_str, "/\\", "&&");
            boost::replace_all(cond_str, "\\/", "||");
            res << "    while " << cond_str << " {\n";
            for (const auto& p: rule.getUpdate(0)) {
                res << "        " << p.first << " = " << p.second << ";\n";
            }
            res << "    }\n";
        }
    }
    res << "    return 0;\n";
    res << "}\n";
    std::cout << res.str() << std::endl;
}

}
