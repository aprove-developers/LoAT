#ifndef COMBINED_SOLVER_HPP
#define COMBINED_SOLVER_HPP

#include <type_traits>
#include <future>
#include "smt.hpp"

template <class S1, class S2>
class CombinedSolver : public Smt {

private:

    enum Active {Fst, Snd, None};

    std::unique_ptr<S1> s1;
    std::unique_ptr<S2> s2;
    Active active = None;

public:

    CombinedSolver(S1* s1, S2* s2): s1(std::unique_ptr<S1>(s1)), s2(std::unique_ptr<S2>(s2)) {
        static_assert(std::is_base_of<Smt, S1>::value, "Derived not derived from BaseClass");
        static_assert(std::is_base_of<Smt, S2>::value, "Derived not derived from BaseClass");
    }

    virtual void add(const BoolExpr e) {
        s1->add(e);
        s2->add(e);
    }

    virtual void push() {
        s1->push();
        s2->push();
    }

    virtual void pop() {
        s1->pop();
        s2->pop();
    }

    virtual Result check() {
        auto s1_async = std::async([&](){return s1->check();});
        Result res = s2->check();
        s1_async.wait();
        if (res != Unknown) {
            active = Snd;
            return res;
        }
        active = Fst;
        return s1_async.get();
    }

    virtual Model model() {
        switch (active) {
        case Fst: return s1->model();
        case Snd: return s2->model();
        case None: throw std::invalid_argument("called combined_solver::model, but no solver is active");
        }
    }

    virtual void setTimeout(unsigned int timeout) {
        s1->setTimeout(timeout);
        s2->setTimeout(timeout);
    }

    virtual void enableModels() {
        s1->enableModels();
        s2->enableModels();
    }

    virtual void resetSolver() {
        s1->resetSolver();
        s2->resetSolver();
        active = None;
    }

    virtual ~CombinedSolver() {}

protected:

    virtual BoolExprSet _unsatCore(const BoolExprSet &assumptions) {
        auto s1_async = std::async([&](){return s1->_unsatCore(assumptions);});
        const BoolExprSet res1 = s2->_unsatCore(assumptions);
        s1_async.wait();
        const BoolExprSet res2 = s1_async.get();
        if (res1.empty() && !res2.empty()) {
            return res2;
        } else if (!res1.empty() && res2.empty()) {
            return res1;
        } else if (res1.size() <= res2.size()) {
            return res1;
        } else {
            return res2;
        }
    }

};

#endif // COMBINED_SOLVER_HPP
