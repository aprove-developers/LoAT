#ifndef SMT_H
#define SMT_H

#include "../expr/expression.hpp"
#include "../expr/boolexpr.hpp"
#include "../its/variablemanager.hpp"
#include "model.hpp"

class Smt
{
public:

    enum Result {Sat, Unknown, Unsat};
    enum Logic {LA, NA, ENA};

    virtual void add(const BoolExpr &e) = 0;
    void add(const Rel &e);
    virtual void push() = 0;
    virtual void pop() = 0;
    virtual Result check() = 0;
    virtual Model model() = 0;
    virtual Subs modelSubs() = 0;
    virtual void setTimeout(unsigned int timeout) = 0;
    virtual void enableModels() = 0;
    virtual void enableUnsatCores() = 0;
    virtual void resetSolver() = 0;
    virtual BoolExpr unsatCore() = 0;
    virtual ~Smt();

    static Smt::Result check(const BoolExpr &e, const VariableManager &varMan);
    static bool isImplication(const BoolExpr &lhs, const BoolExpr &rhs, const VariableManager &varMan);
    static Logic chooseLogic(const std::vector<BoolExpr> &xs, const std::vector<Subs> &up = {});

    template<class RELS, class UP> static Logic chooseLogic(const std::vector<RELS> &g, const std::vector<UP> &up) {
        Logic res = LA;
        for (const RELS &rels: g) {
            for (const Rel &rel: rels) {
                if (!rel.isLinear()) {
                    if (!rel.isPoly()) {
                        return ENA;
                    }
                    res = NA;
                }
            }
        }
        for (const UP &t: up) {
            if (!t.isLinear()) {
                if (!t.isPoly()) {
                    return ENA;
                }
                res = NA;
            }
        }
        return res;
    }

};

#endif // SMT_H
