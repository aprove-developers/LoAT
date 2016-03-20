#ifndef LIMITPROBLEM_H
#define LIMITPROBLEM_H

#include <ostream>
#include <set>
#include <utility>

#include "exceptions.h"
#include "expression.h"
#include "guardtoolbox.h"


enum InftyDirection { POS_INF = 0, NEG_INF, POS_CONS, NEG_CONS, POS };

class LimitVector {
public:
    LimitVector(InftyDirection type, InftyDirection first, InftyDirection second);

    InftyDirection getType() const;
    InftyDirection getFirst() const;
    InftyDirection getSecond() const;

    bool isApplicable(InftyDirection dir) const;

    static const std::vector<LimitVector> Addition;
    static const std::vector<LimitVector> Multiplication;
    static const std::vector<LimitVector> Division;

private:
    const InftyDirection type;
    const InftyDirection first;
    const InftyDirection second;
};

std::ostream& operator<<(std::ostream &, const LimitVector &);

class InftyExpression : public Expression {
public:

    InftyExpression(InftyDirection dir = POS);
    InftyExpression(const GiNaC::basic &other, InftyDirection dir = POS);
    InftyExpression(const GiNaC::ex &other, InftyDirection dir = POS);

    void setDirection(InftyDirection dir);
    InftyDirection getDirection() const;

    void dump(const std::string &description) const;

private:
    InftyDirection direction;

};

typedef std::set<InftyExpression, GiNaC::ex_is_less> InftyExpressionSet;

class LimitProblem {
public:
    LimitProblem();
    LimitProblem(const GuardList &normalizedGuard, const Expression &cost);

    LimitProblem(const LimitProblem &other);
    LimitProblem& operator=(const LimitProblem &other);

    LimitProblem(LimitProblem &&other);
    LimitProblem& operator=(LimitProblem &&other);

    void addExpression(const InftyExpression &ex);

    InftyExpressionSet::const_iterator cbegin() const;
    InftyExpressionSet::const_iterator cend() const;

    // (A)
    void applyLimitVector(const InftyExpressionSet::const_iterator &it, int pos,
                          const LimitVector &lv);
    void applyLimitVectorAdvanced(const InftyExpressionSet::const_iterator &it, Expression l, Expression r,
                          const LimitVector &lv);
    // (B)
    void removeConstant(const InftyExpressionSet::const_iterator &it);
    // (C)
    void substitute(const GiNaC::exmap &sub, int substitutionIndex);
    // (D)
    void trimPolynomial(const InftyExpressionSet::const_iterator &it);
    // (E)
    void reducePolynomialPower(const InftyExpressionSet::const_iterator &it);
    // (F)
    void reduceGeneralPower(const InftyExpressionSet::const_iterator &it);

    bool isSolved() const;
    GiNaC::exmap getSolution() const;
    ExprSymbol getN() const;
    const std::vector<int>& getSubstitutions() const;
    InftyExpressionSet::const_iterator find(const InftyExpression &ex);
    std::vector<Expression> getQuery();
    bool isUnsat();
    bool isUnsolvable() const;
    void setUnsolvable();

    bool removeConstantIsApplicable(const InftyExpressionSet::const_iterator &it);
    bool trimPolynomialIsApplicable(const InftyExpressionSet::const_iterator &it);
    bool reducePolynomialPowerIsApplicable(const InftyExpressionSet::const_iterator &it);
    bool reduceGeneralPowerIsApplicable(const InftyExpressionSet::const_iterator &it);

    //debug dumping
    void dump(const std::string &description);

public:
    InftyExpressionSet set;
    ExprSymbol variableN;
    std::vector<int> substitutions;
    std::vector<std::string> log;
    bool unsolvable;
};

#endif //LIMITPROBLEM_H