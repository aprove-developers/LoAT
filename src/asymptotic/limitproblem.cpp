#include "limitproblem.hpp"

#include <sstream>
#include <utility>

#include "../expr/relation.hpp"
#include "../smt/smt.hpp"

using namespace GiNaC;

LimitProblem::LimitProblem(VariableManager &varMan)
    : variableN("n"), unsolvable(false), varMan(varMan), log(new std::ostringstream()) {
}


LimitProblem::LimitProblem(const GuardList &normalizedGuard, const Expression &cost, VariableManager &varMan)
    : variableN("n"), unsolvable(false), varMan(varMan), log(new std::ostringstream()) {
    for (const Rel &rel : normalizedGuard) {
        assert(rel.isGreaterThanZero());

        addExpression(InftyExpression(rel.lhs(), POS));
    }

    assert(!cost.isRelation());
    addExpression(InftyExpression(cost, POS_INF));

    (*log) << "Created initial limit problem:" << std::endl
        << *this << std::endl << std::endl;
}


LimitProblem::LimitProblem(const GuardList &normalizedGuard, VariableManager &varMan)
    : variableN("n"), unsolvable(false), varMan(varMan), log(new std::ostringstream()) {
    for (const Rel &rel : normalizedGuard) {
        assert(rel.isGreaterThanZero());
        addExpression(InftyExpression(rel.lhs(), POS));
    }

    (*log) << "Created initial limit problem without cost:" << std::endl
        << *this << std::endl << std::endl;
}


LimitProblem::LimitProblem(const LimitProblem &other)
    : set(other.set),
      variableN(other.variableN),
      substitutions(other.substitutions),
      unsolvable(other.unsolvable),
      varMan(other.varMan),
      log(new std::ostringstream()) {
    (*log) << other.log->str();
}


LimitProblem& LimitProblem::operator=(const LimitProblem &other) {
    if (this != &other) {
        set = other.set;
        variableN = other.variableN;
        substitutions = other.substitutions;
        varMan = other.varMan;
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
      varMan(other.varMan),
      log(std::move(other.log)) {
}


LimitProblem& LimitProblem::operator=(LimitProblem &&other) {
    if (this != &other) {
        set = std::move(other.set);
        variableN = other.variableN;
        substitutions = std::move(other.substitutions);
        varMan = other.varMan;
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

    set.erase(it);
    addExpression(firstIE);
    addExpression(secondIE);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
}


void LimitProblem::removeConstant(const InftyExpressionSet::const_iterator &it) {
    assert(it->isIntegerConstant());

    GiNaC::numeric num = it->toNumeric();
    Direction dir = it->getDirection();
    assert((num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS));

    (*log) << "applying transformation rule (B), deleting " << *it << std::endl;

    set.erase(it);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
}


void LimitProblem::substitute(const ExprMap &sub, int substitutionIndex) {
    for (auto const &s : sub) {
        assert(!s.second.has(s.first));
    }

    (*log) << "applying transformation rule (C) using substitution " << sub << std::endl;

    InftyExpressionSet oldSet;
    oldSet.swap(set);
    for (const InftyExpression &ex : oldSet) {
        addExpression(InftyExpression(ex.subs(sub), ex.getDirection()));
    }

    substitutions.push_back(substitutionIndex);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
}


void LimitProblem::trimPolynomial(const InftyExpressionSet::const_iterator &it) {
    // the expression has to be a univariate polynomial
    assert(it->isPolynomial());
    assert(it->hasExactlyOneVariable());

    Direction dir = it->getDirection();
    assert((dir == POS) || (dir == POS_INF) || (dir == NEG_INF));

    ExprSymbol var = it->getAVariable();

    Expression expanded = it->expand();

    if (expanded.isAdd()) {
        Expression leadingTerm = expanded.lcoeff(var) * pow(var, expanded.degree(var));

        if (dir == POS) {
            // Fix the direction
            dir = POS_INF;
        }

        InftyExpression inftyExp(leadingTerm, dir);

        (*log) << "applying transformation rule (D), replacing " << *it
            << " by " << inftyExp << std::endl;

        set.erase(it);
        addExpression(inftyExp);
    }

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
}


void LimitProblem::reduceExp(const InftyExpressionSet::const_iterator &it) {
    assert(it->getDirection() == POS_INF || it->getDirection() == POS);

    assert(it->hasExactlyOneVariable());
    ExprSymbol x = it->getAVariable();

    Expression powerInExp;
    if (it->isAdd()) {
        for (unsigned int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (summand.isPower() && summand.op(1).has(x)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }
    assert(powerInExp.isPower());

    Expression b = *it - powerInExp;
    assert(b.is_polynomial(x));

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    assert(a.is_polynomial(x));
    assert(e.is_polynomial(x));
    assert(e.has(x));

    InftyExpression firstIE(a - 1, POS);
    InftyExpression secondIE(e, POS_INF);

    (*log) << "applying transformation rule (E), replacing " << *it << " by "
        << firstIE << " and " << secondIE << std::endl;

    set.erase(it);
    addExpression(firstIE);
    addExpression(secondIE);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
}


void LimitProblem::reduceGeneralExp(const InftyExpressionSet::const_iterator &it) {
    assert(it->getDirection() == POS_INF || it->getDirection() == POS);

    Expression powerInExp;
    if (it->isAdd()) {
        for (unsigned int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (summand.isPower() && (!summand.op(1).isPolynomial()
                                         || summand.hasAtLeastTwoVariables())) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }
    assert(powerInExp.isPower());
    assert(!powerInExp.op(1).isPolynomial()
           || powerInExp.hasAtLeastTwoVariables());

    Expression b = *it - powerInExp;

    Expression a = powerInExp.op(0);
    Expression e = powerInExp.op(1);

    InftyExpression firstIE(a - 1, POS);
    InftyExpression secondIE(e + b, POS_INF);

    (*log) << "reducing general power, replacing " << *it
        << " by " << firstIE << " and " << secondIE << std::endl;

    set.erase(it);
    addExpression(firstIE);
    addExpression(secondIE);

    (*log) << "resulting limit problem:" << std::endl << *this << std::endl << std::endl;
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
        if (!ex.isSymbol()) {
            return false;
        }
    }

    // Since the infinity expressions are compared using GiNaC::ex_is_less,
    // the directions do not affect the comparison.
    // Therefore, there cannot be a variable with different directions.
    return true;
}


ExprMap LimitProblem::getSolution() const {
    assert(isSolved());

    ExprMap solution;
    for (const InftyExpression &ex : set) {
        switch (ex.getDirection()) {
            case POS:
            case POS_INF:
                solution.put(ex, variableN);
                break;
            case NEG_INF:
                solution.put(ex, -variableN);
                break;
            case POS_CONS:
                solution.put(ex, 1);
                break;
            case NEG_CONS:
                solution.put(ex, -1);
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


std::vector<Rel> LimitProblem::getQuery() const {
    std::vector<Rel> query;

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
    return Smt::check(buildAnd(getQuery()), varMan) == Smt::Unsat;
}


bool LimitProblem::isLinear() const {
    for (const InftyExpression &ex : set) {
        if (!ex.isLinear()) {
            return false;
        }
    }
    return true;
}

bool LimitProblem::isPolynomial() const {
    for (const InftyExpression &ex : set) {
        if (!ex.isPolynomial()) {
            return false;
        }
    }
    return true;
}

bool LimitProblem::removeConstantIsApplicable(const InftyExpressionSet::const_iterator &it) {
    if (!it->isIntegerConstant()) {
        return false;
    }

    GiNaC::numeric num = it->toNumeric();
    Direction dir = it->getDirection();

    return (num.is_positive() && (dir == POS_CONS || dir == POS))
           || (num.is_negative() && dir == NEG_CONS);
}


bool LimitProblem::trimPolynomialIsApplicable(const InftyExpressionSet::const_iterator &it) {
    Direction dir = it->getDirection();
    if (!((dir == POS) || (dir == POS_INF) || (dir == NEG_INF))) {
        return false;
    }

    if (!it->isPolynomial()) {
        return false;
    }

    // Check if it is a monom
    if (!it->expand().isAdd()) {
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
    if (it->isAdd()) {
        for (unsigned int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (summand.isPower() && summand.op(1).has(x)) {
                powerInExp = summand;
                break;
            }
        }

    } else {
        powerInExp = *it;
    }

    if (!powerInExp.isPower()) {
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

    if (it->isAdd()) {
        for (unsigned int i = 0; i < it->nops(); ++i) {
            Expression summand = it->op(i);
            if (summand.isPower() && (!summand.op(1).isPolynomial()
                                         || summand.hasAtLeastTwoVariables())) {
                return true;
            }
        }

        return false;
    }

    return it->isPower() && (!it->op(1).isPolynomial()
                                || it->hasAtLeastTwoVariables());
}

InftyExpressionSet::size_type LimitProblem::getSize() const {
    return set.size();
}

const InftyExpressionSet LimitProblem::getSet() const {
    return set;
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
