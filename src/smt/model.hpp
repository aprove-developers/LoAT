#ifndef SMT_MODEL_HPP
#define SMT_MODEL_HPP

#include "../expr/boolexpr.hpp"

class Model
{
public:

    Model(VarMap<GiNaC::numeric> vars);

    GiNaC::numeric get(const Var &var) const;
    bool contains(const Var &var) const;

    friend std::ostream& operator<<(std::ostream &s, const Model &e);

private:

    VarMap<GiNaC::numeric> vars;

};

#endif // SMT_MODEL_HPP
