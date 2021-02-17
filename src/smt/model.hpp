#ifndef MODEL_HPP
#define MODEL_HPP

#include "../expr/boolexpr.hpp"

class Model
{
public:

    Model(VarMap<GiNaC::numeric> vars, std::map<uint, bool> constants);

    GiNaC::numeric get(const Var &var) const;
    bool get(uint id) const;
    bool contains(const Var &var) const;
    bool contains(uint id) const;
    Subs toSubs() const;

    friend std::ostream& operator<<(std::ostream &s, const Model &e);

private:

    VarMap<GiNaC::numeric> vars;
    std::map<uint, bool> constants;

};

#endif // MODEL_HPP
