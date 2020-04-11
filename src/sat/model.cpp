#include "model.hpp"
#include <sstream>

namespace sat {

Model::Model(std::map<uint, bool> vars): vars(vars) {}

bool Model::get(uint i) const {
    return vars.at(i);
}

bool Model::contains(uint i) const {
    return vars.count(i) > 0;
}

std::ostream& operator<<(std::ostream &s, const Model &e) {
    bool first = true;
    s << "{";
    for (const auto &p: e.vars) {
        if (first) {
            s << p.first << "=" << p.second;
            first = false;
        } else s << ", " << p.first << "=" << p.second;
    }
    return s << " }";
}

}
