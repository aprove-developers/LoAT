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
