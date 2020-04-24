#ifndef Z3_HPP
#define Z3_HPP

#include "../sat.hpp"
#include "../../config.hpp"
#include <z3++.h>

namespace sat {

class Z3: public Sat
{
public:

    Z3();
    void add(const PropExpr &e) override;
    SatResult check() override;
    Model model() const override;
    void setTimeout(const uint timeout) override;
    void push() override;
    void pop() override;
    std::set<std::set<int>> cnf(const PropExpr &e);

private:

    z3::context ctx;
    z3::solver solver;
    std::map<uint, z3::expr> vars;
    std::map<std::string, uint> varNames;

    z3::expr convert(const PropExpr &e);
    std::set<int> convert(const z3::expr &e);

};

}

#endif // Z3_HPP
