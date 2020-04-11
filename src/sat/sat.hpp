#ifndef SAT_HPP
#define SAT_HPP

#include "propexpr.hpp"
#include "model.hpp"
#include "../util/satresult.hpp"

namespace sat {

class Sat {

public:

    virtual void add(const PropExpr &e) = 0;
    virtual SatResult check() const = 0;
    virtual Model model() const = 0;
    virtual void push() = 0;
    virtual void pop() = 0;
    virtual void setTimeout(const uint timeout) = 0;

    virtual ~Sat();

};

class MaxSat: public Sat {

public:

    virtual void soft(const PropExpr &e) = 0;

};

}

#endif // SAT_HPP
