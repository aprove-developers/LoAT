#ifndef YICES_SAT_HPP
#define YICES_SAT_HPP

#include <yices.h>
#include "../sat.hpp"
#include "../../config.hpp"

namespace sat {

class Yices: public Sat {

public:

    Yices();
    ~Yices() override;
    void add(const PropExpr &e) override;
    SatResult check() const override;
    Model model() const override;
    void setTimeout(const uint timeout) override;
    void push() override;
    void pop() override;
    term_t convert(const PropExpr &e);

private:

    context_t *solver;
    std::map<int, term_t> vars;
    unsigned int timeout = Config::Smt::DefaultTimeout;

};

}

#endif // YICES_SAT_HPP
