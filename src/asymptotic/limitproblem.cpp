#include "limitproblem.h"

#include <z3++.h>
#include <sstream>
#include <utility>

#include "debug.h"
#include "expr/relation.h"
#include "z3/z3toolbox.h"

using namespace GiNaC;

LimitProblem::LimitProblem()
    : variableN("n"), unsolvable(false), log(new std::ostringstream()) {
}


LimitProblem::LimitProblem(const GuardList &normalizedGuard, const Expression &cost)
    : variableN("n"), unsolvable(false), log(new std::ostringstream()) {
    for (const Expression &ex : normalizedGuard) {
        assert(Relation::isGreaterThanZero(ex));

        addExpression(InftyExpression(ex.lhs(), POS));
    }

    assert(!is_a<relational>(cost));
    addExpression(InftyExpression(cost, POS_INF));

    (*log) << "Created initial limit problem:" << std::endl
        << *this << std::endl << std::endl;
}


LimitProblem::LimitProblem(const GuardList &normalizedGuard)
    : variableN("n"), unsolvable(false), log(new std::ostringstream()) {
    for (const Expression &ex : normalizedGuard) {
        assert(Relation::isGreaterThanZero(ex));
        addExpression(InftyExpression(ex.lhs(), POS));
    }

    (*log) << "Created initial limit problem without cost:" << std::endl
        << *this << std::endl << std::endl;
}


LimitProblem::LimitProblem(const LimitProblem &other)
    : set(other.set),
      variableN(other.variableN),
      substitutions(other.substitutions),
      unsolvable(other.unsolvable),
      log(new std::ostringstream()) {
    (*log) << other.log->str();
}


LimitProblem& LimitProblem::operator=(const LimitProblem &other) {
    if (this != &other) {
        set = other.set;
        variableN = other.variableN;
        substitutions = other.substitutions;
        (*log) << other.log->str();
        unsolvable = other.unsolvable;
    }

    return *this;
}


LimitProblem::LimitProblem(LimitProblem &&other)
    : set(std::move(other.set)),
      variableN(other.variableN),
      substitutions(std::move(other.substitutions)),
      unsolvable(other.unsolvable),
      log(std::move(other.log)) {
}


LimitProblem& LimitProblem::operator=(LimitProblem &&other) {
    if (this != &other) {
        set = std::move(other.set);
        variableN = other.variableN;
        substitutions = std::move(other.substitutions);
        log = std::move(other.log);
        unsolvable = other.unsolvable;
    }

    return *this;
}


void LimitProblem::addExpression(const InftyExpression &ex) {
    InftyExpressionSet::iterator it = set.find(ex);

    if (it == set.end()) {
        // ex is not present
        set.insert(ex);

    } else {
        // ex is already present
        if (it->getDirection() != ex.getDirection()) {
            if (it->getDirection() == POS &&
                (ex.getDirection() == POS_INF || ex.getDirection() == POS_CONS)) {
                // fix direction
                set.erase(it);
                set.insert(ex);

            } else if (!(ex.getDirection() == POS &&
                         (it->getDirection() == POS_INF || it->getDirection() == POS_CONS))) {
                // the limit problem is contradictory
                unsolvable = true;
            }
        }
    }

    // check if the expression is unsatisfiable
    if (ex.isTriviallyUnsatisfiable()) {
        unsolvable = true;
    }
}


InftyExpressionSet::iterator LimitProblem::cbegin() const {
    return set.cbegin();
}


InftyExpressionSet::iterator LimitProblem::cend() const {
    return set.cend();
}


void LimitProblem::applyLimitVector(const InftyExpressionSet::const_iterator &it,
                                    const Expression &l, const Expression &r,
                                    const LimitVector &lv) {
    Direction dir = it->getDirection();
    assert(lv.isApplicable(dir));

    InftyExpression firstIE(l, lv.getFirst());
    InftyExpression secondIE(r, lv.getSecond());

    (*log) << "applying transformation rule (A), replacing " << *it
        << " by " << firstIE << " and " << secondIE << " using " << lv << std::endl;
    debugLimitProblem("applying transformation rule (A), replacing " << *it
                      << " by " << firstIE << " and " << secondIE << " using " << lv);


    set.erase(it);
    addExpression(firstIE);
    addExpression(secondIE);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
    debugLimitProblem("");
}


void LimitProblem::removeConstant(const InftyExpressionSet::const_iterator &it) {
    assert(it->info(info_flags::integer));

    numeric num = ex_to<numeric>(*it);
    Direction dir = it->getDirection();
    assert((num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS));

    (*log) << "applying transformation rule (B), deleting " << *it << std::endl;
    debugLimitProblem("applying transformation rule (B), deleting " << *it);

    set.erase(it);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
    debugLimitProblem("");
}


void LimitProblem::substitute(const GiNaC::exmap &sub, int substitutionIndex) {
    for (auto const &s : sub) {
        assert(!s.second.has(s.first));
    }

    (*log) << "applying transformation rule (C) using substitution " << sub << std::endl;
    debugLimitProblem("applying transformation rule (C) using substitution " << sub);

    InftyExpressionSet oldSet;
    oldSet.swap(set);
    for (const InftyExpression &ex : oldSet) {
        addExpression(InftyExpression(ex.subs(sub), ex.getDirection()));
    }

    substitutions.push_back(substitutionIndex);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
    debugLimitProblem("");
}


void LimitProblem::trimPolynomial(const InftyExpressionSet::const_iterator &it) {
    // the expression has to be a univariate polynomial
    assert(it->info(info_flags::polynomial));
    assert(it->hasExactlyOneVariable());

    Direction dir = it->getDirection();
    assert((dir == POS) || (dir == POS_INF) || (dir == NEG_INF));

    ExprSymbol var = it->getAVariable();

    Expression expanded = it->expand();
    debugLimitProblem("expanded " << *it << " to " << expanded);


    if (is_a<add>(expanded)) {
        Expression leadingTerm = expanded.lcoeff(var) * pow(var, expanded.degree(var));

        debugLimitProblem("the leading term is " << leadingTerm);

        if (dir == POS) {
            // Fix the direction
            dir = POS_INF;
        }

        InftyExpression inftyExp(leadingTerm, dir);

        (*log) << "applying transformation rule (D), replacing " << *it
            << " by " << inftyExp << std::endl;
        debugLimitProblem("applying transformation rule (D), replacing " << *it
                          << " by " << inftyExp);

        set.erase(it);
        addExpression(inftyExp);
    } else {
        debugLimitProblem(*it << " is already a monom");
    }

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
    debugLimitProblem("");
}


void LimitProblem::reduceExp(const InftyExpressionSet::const_iterator &it) {
    assert(it->getDirection() == POS_INF || it->getDirection() == POS);

    debugLimitProblem("expression: " << *it);

    assert(it->hasExactlyOneVariable());
    ExprSymbol x = it->getAVariable();

    Expression powerInExp;
    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && summand.op(1).has(x)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }
    debugLimitProblem("exp: " << powerInExp);
    assert(is_a<power>(powerInExp));

    Expression b = *it - powerInExp;
    debugLimitProblem("b: " << b);
    assert(b.is_polynomial(x));

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    debugLimitProblem("a: " << a << ", e: " << e);

    assert(a.is_polynomial(x));
    assert(e.is_polynomial(x));
    assert(e.has(x));

    InftyExpression firstIE(a - 1, POS);
    InftyExpression secondIE(e, POS_INF);

    (*log) << "applying transformation rule (E), replacing " << *it << " by "
        << firstIE << " and " << secondIE << std::endl;
    debugLimitProblem("applying transformation rule (E), replacing " << *it
                      << " by " << firstIE << " and " << secondIE);

    set.erase(it);
    addExpression(firstIE);
    addExpression(secondIE);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
    debugLimitProblem("");
}


void LimitProblem::reduceGeneralExp(const InftyExpressionSet::const_iterator &it) {
    assert(it->getDirection() == POS_INF || it->getDirection() == POS);

    debugLimitProblem("expression: " << *it);

    Expression powerInExp;
    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && (!summand.op(1).info(info_flags::polynomial)
                                         || summand.hasAtLeastTwoVariables())) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }
    debugLimitProblem("general power: " << powerInExp);
    assert(is_a<power>(powerInExp));
    assert(!powerInExp.op(1).info(info_flags::polynomial)
           || powerInExp.hasAtLeastTwoVariables());

    Expression b = *it - powerInExp;
    debugLimitProblem("b: " << b);

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    debugLimitProblem("a: " << a << ", e: " << e);

    InftyExpression firstIE(a - 1, POS);
    InftyExpression secondIE(e + b, POS_INF);

    (*log) << "reducing general power, replacing " << *it
        << " by " << firstIE << " and " << secondIE << std::endl;
    debugLimitProblem("reducing general power, replacing " << *it
                      << " by " << firstIE << " and " << secondIE);

    set.erase(it);
    addExpression(firstIE);
    addExpression(secondIE);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
    debugLimitProblem("resulting limit problem:");
    debugLimitProblem(*this);
    debugLimitProblem("");
}


void LimitProblem::removeAllConstraints() {
    (*log) << "removing all constraints (solved by SMT)" << std::endl;
    set.clear();
    (*log) << "resulting limit problem: " << *this << std::endl << std::endl;
}


bool LimitProblem::isUnsolvable() const {
    return unsolvable;
}


void LimitProblem::setUnsolvable() {
    unsolvable = true;
}

bool LimitProblem::isSolved() const {
    if (isUnsolvable()) {
        return false;
    }

    // Check if an expression is not a variable
    for (const InftyExpression &ex : set) {
        if (!is_a<symbol>(ex)) {
            return false;
        }
    }

    // Since the infinity expressions are compared using GiNaC::ex_is_less,
    // the directions do not affect the comparison.
    // Therefore, there cannot be a variable with different directions.
    return true;
}


GiNaC::exmap LimitProblem::getSolution() const {
    assert(isSolved());

    exmap solution;
    for (const InftyExpression &ex : set) {
        switch (ex.getDirection()) {
            case POS:
            case POS_INF:
                solution.insert(std::make_pair(ex, variableN));
                break;
            case NEG_INF:
                solution.insert(std::make_pair(ex, -variableN));
                break;
            case POS_CONS:
                solution.insert(std::make_pair(ex, numeric(1)));
                break;
            case NEG_CONS:
                solution.insert(std::make_pair(ex, numeric(-1)));
                break;
        }
    }

    return solution;
}


ExprSymbol LimitProblem::getN() const {
    return variableN;
}


const std::vector<int>& LimitProblem::getSubstitutions() const {
    return substitutions;
}


InftyExpressionSet::const_iterator LimitProblem::find(const InftyExpression &ex) const {
    return set.find(ex);
}


ExprSymbolSet LimitProblem::getVariables() const {
    ExprSymbolSet res;
    for (const InftyExpression &ex : set) {
        ExprSymbolSet exVars = ex.getVariables();
        res.insert(exVars.cbegin(),exVars.cend());
    }
    return res;
}


std::vector<Expression> LimitProblem::getQuery() const {
    std::vector<Expression> query;

    for (const InftyExpression &ex : set) {
        if (ex.getDirection() == NEG_INF || ex.getDirection() == NEG_CONS) {
            query.push_back(ex.expand() < 0);
        } else {
            query.push_back(ex.expand() > 0);
        }
    }

    return query;
}


bool LimitProblem::isUnsat() const {
    return Z3Toolbox::checkAll(getQuery()) == z3::unsat;
}


bool LimitProblem::isLinear(const GiNaC::lst &vars) const {
    for (const InftyExpression &ex : set) {
        if (!ex.isLinear(vars)) {
            return false;
        }
    }
    return true;
}

bool LimitProblem::isPolynomial(const GiNaC::lst &vars) const {
    for (const InftyExpression &ex : set) {
        if (!ex.is_polynomial(vars)) {
            return false;
        }
    }
    return true;
}

bool LimitProblem::removeConstantIsApplicable(const InftyExpressionSet::const_iterator &it) {
    if (!it->info(info_flags::integer)) {
        return false;
    }

    numeric num = ex_to<numeric>(*it);
    Direction dir = it->getDirection();

    return (num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS);
}


bool LimitProblem::trimPolynomialIsApplicable(const InftyExpressionSet::const_iterator &it) {
    Direction dir = it->getDirection();
    if (!((dir == POS) || (dir == POS_INF) || (dir == NEG_INF))) {
        return false;
    }

    if (!it->info(info_flags::polynomial)) {
        return false;
    }

    // Check if it is a monom
    if (!is_a<add>(it->expand())) {
        return false;
    }

    return it->hasExactlyOneVariable();
}


bool LimitProblem::reduceExpIsApplicable(const InftyExpressionSet::const_iterator &it) {
    Direction dir = it->getDirection();
    if (!(dir == POS_INF || dir == POS)) {
        return false;
    }

    if (!it->hasExactlyOneVariable()) {
        return false;
    }

    ExprSymbol x = it->getAVariable();

    Expression powerInExp;
    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && summand.op(1).has(x)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }

    if (!is_a<power>(powerInExp)) {
        return false;
    }

    Expression b = *it - powerInExp;

    if (!b.is_polynomial(x)) {
        return false;
    }

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    if (!(a.is_polynomial(x) && e.is_polynomial(x))) {
        return false;
    }

    return e.has(x);
}


bool LimitProblem::reduceGeneralExpIsApplicable(const InftyExpressionSet::const_iterator &it) {
    Direction dir = it->getDirection();
    if (!(dir == POS_INF || dir == POS)) {
        return false;
    }

    if (is_a<add>(*it)) {
        for (int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (is_a<power>(summand) && (!summand.op(1).info(info_flags::polynomial)
                                         || summand.hasAtLeastTwoVariables())) {
                return true;
            }
        }

        return false;
    }

    return is_a<power>(*it) && (!it->op(1).info(info_flags::polynomial)
                                || it->hasAtLeastTwoVariables());
}

InftyExpressionSet::size_type LimitProblem::getSize() const {
    return set.size();
}


std::string LimitProblem::getProof() const {
    return log->str();
}


std::ostream& operator<<(std::ostream &os, const LimitProblem &lp) {
    if (lp.cbegin() != lp.cend()) {
        std::copy(lp.cbegin(), --lp.cend(), std::ostream_iterator<InftyExpression>(os, ", "));
        os << *(--lp.cend()) << " ";
    }

    os << "[" << (lp.isUnsolvable() ? "unsolvable"
                  : (lp.isSolved() ? "solved" : "not solved")) << "]";

    return os;
}
