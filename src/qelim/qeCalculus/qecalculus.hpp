#ifndef QECALCULUS_HPP
#define QECALCULUS_HPP

#include "../qelim.hpp"
#include "../../util/proof.hpp"
#include "../../smt/smt.hpp"

class QeProblem : public Qelim
{

private:

    struct Entry {
        RelSet dependencies;
        BoolExpr formula;
        bool exact;
    };

    using Res = RelMap<std::vector<Entry>>;

    Res res;
    option<RelMap<Entry>> solution;
    RelSet todo;
    Proof proof;
    std::unique_ptr<Smt> solver;
    option<QuantifiedFormula> formula;
    BoolExpr boundedFormula;
    VariableManager &varMan;
    bool isConjunction;

    bool monotonicity(const Rel &rel, const Var &n);
    bool recurrence(const Rel &rel, const Var &n);
    bool eventualWeakDecrease(const Rel &rel, const Var &n);
    bool eventualWeakIncrease(const Rel &rel, const Var &n);
    bool fixpoint(const Rel &rel, const Var &x);
    RelSet findConsistentSubset(const BoolExpr e) const;
    option<unsigned int> store(const Rel &rel, const BoolExpr formula, bool exact = true);

    struct ReplacementMap {
        bool exact;
        RelMap<BoolExpr> map;
    };

    ReplacementMap computeReplacementMap(bool nontermOnly) const;
    Quantifier getQuantifier() const;

public:

    struct Result {
        BoolExpr qf;
        bool exact;

        Result(const BoolExpr &qf, bool exact): qf(qf), exact(exact) {}

    };

    QeProblem(VariableManager &varMan): varMan(varMan){}

    std::vector<Result> computeRes();
    std::pair<BoolExpr, bool> buildRes(const Model &model, const std::map<Rel, std::vector<BoolExpr>> &entryVars);
    Proof getProof() const;

    option<BoolExpr> qe(const QuantifiedFormula &qf) override;

};

#endif // QECALCULUS_HPP
