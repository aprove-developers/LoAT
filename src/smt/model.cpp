#include "model.hpp"

Model::Model(VarMap<GiNaC::numeric> vars, std::map<uint, bool> constants): vars(vars), constants(constants) {}

GiNaC::numeric Model::get(const Var &var) const {
    return vars.at(var);
}

bool Model::get(uint id) const {
    return constants.at(id);
}

bool Model::contains(const Var &var) const {
    return vars.count(var) > 0;
}

bool Model::contains(uint id) const {
    return constants.count(id) > 0;
}

Subs Model::toSubs() const {
    Subs res;
    for (const auto &e: vars) {
        res.put(e.first, e.second);
    }
    return res;
}

std::ostream& operator<<(std::ostream &s, const Model &e) {
    if (!e.vars.empty() && !e.constants.empty()) {
        s << "variables:" << std::endl;
    }
    for (const auto &p: e.vars) {
        s << " " << p.first << "=" << p.second;
    }
    s << std::endl;
    if (!e.vars.empty() && !e.constants.empty()) {
        s << "constants:" << std::endl;
    }
    for (const auto &p: e.constants) {
        s << " " << p.first << "=" << p.second;
    }
    s << std::endl;
    return s;
}
