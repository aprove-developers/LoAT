/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#include "relation.hpp"


namespace Relation {

    bool isRelation(const Expression &ex) {
        return GiNaC::is_a<GiNaC::relational>(ex)
               && ex.nops() == 2
               && !ex.info(GiNaC::info_flags::relation_not_equal);
    }

    bool isPolynomial(const Expression &ex) {
        if (!isRelation(ex)) return false;
        return Expression(ex.lhs()).isPolynomial() && Expression(ex.rhs()).isPolynomial();
    }

    bool isEquality(const Expression &ex) {
        return isRelation(ex) && ex.info(GiNaC::info_flags::relation_equal);
    }

    bool isInequality(const Expression &ex) {
        return isRelation(ex) && !isEquality(ex);
    }

    bool isLinearInequality(const Expression &ex, const boost::optional<ExprSymbolSet> &vars) {
        if (!isInequality(ex)) return false;
        return Expression(ex.lhs()).isLinear(vars) && Expression(ex.rhs()).isLinear(vars);
    }

    bool isLinearEquality(const Expression &ex, const boost::optional<ExprSymbolSet> &vars) {
        if (!isEquality(ex)) return false;
        return Expression(ex.lhs()).isLinear(vars) && Expression(ex.rhs()).isLinear(vars);
    }

    bool isGreaterThanZero(const Expression &ex) {
        return isInequality(ex)
               && ex.info(GiNaC::info_flags::relation_greater)
               && ex.rhs().is_zero();
    }

    bool isLessOrEqual(const Expression &ex) {
        return isInequality(ex) && ex.info(GiNaC::info_flags::relation_less_or_equal);
    }

    Expression replaceLhsRhs(const Expression &rel, Expression lhs, Expression rhs) {
        assert(isRelation(rel));
        if (rel.info(GiNaC::info_flags::relation_less_or_equal)) return lhs <= rhs;
        if (rel.info(GiNaC::info_flags::relation_greater)) return lhs > rhs;
        if (rel.info(GiNaC::info_flags::relation_less)) return lhs < rhs;
        if (rel.info(GiNaC::info_flags::relation_greater_or_equal)) return lhs >= rhs;
        if (rel.info(GiNaC::info_flags::relation_equal)) return lhs == rhs;
        throw new std::runtime_error("Unknown relation!");
    }

    Expression toLessEq(Expression rel) {
        assert(isInequality(rel));

        //flip > or >=
        if (rel.info(GiNaC::info_flags::relation_greater)) {
            rel = rel.rhs() < rel.lhs();
        } else if (rel.info(GiNaC::info_flags::relation_greater_or_equal)) {
            rel = rel.rhs() <= rel.lhs();
        }

        //change < to <=, assuming integer arithmetic
        if (rel.info(GiNaC::info_flags::relation_less)) {
            rel = rel.lhs() <= (rel.rhs() - 1);
        }

        assert(rel.info(GiNaC::info_flags::relation_less_or_equal));
        return rel;
    }

    Expression toGreater(Expression rel) {
        assert(isInequality(rel));

        //flip < or <=
        if (rel.info(GiNaC::info_flags::relation_less)) {
            rel = rel.rhs() > rel.lhs();
        } else if (rel.info(GiNaC::info_flags::relation_less_or_equal)) {
            rel = rel.rhs() >= rel.lhs();
        }

        //change >= to >, assuming integer arithmetic
        if (rel.info(GiNaC::info_flags::relation_greater_or_equal)) {
            rel = (rel.lhs() + 1) > rel.rhs();
        }

        assert(rel.info(GiNaC::info_flags::relation_greater));
        return rel;
    }

    Expression normalizeInequality(Expression rel) {
        assert(isInequality(rel));

        Expression greater = toGreater(rel);
        Expression normalized = (greater.lhs() - greater.rhs()) > 0;

        assert(isGreaterThanZero(normalized));
        return normalized;
    }

    Expression toLessOrLessEq(Expression rel) {
        assert(rel.info(GiNaC::info_flags::relation_equal)
               || isInequality(rel));

        if (rel.info(GiNaC::info_flags::relation_greater_or_equal)) {
            rel = rel.rhs() <= rel.lhs();
        } else if (rel.info(GiNaC::info_flags::relation_greater)) {
            rel = rel.rhs() < rel.lhs();
        }

        return rel;
    }

    Expression splitVariablesAndConstants(const Expression &rel, const ExprSymbolSet &params) {
        assert(isInequality(rel));

        //move everything to lhs
        GiNaC::ex newLhs = rel.lhs() - rel.rhs();
        GiNaC::ex newRhs = 0;

        //move all numerical constants back to rhs
        newLhs = newLhs.expand();
        if (GiNaC::is_a<GiNaC::add>(newLhs)) {
            for (size_t i=0; i < newLhs.nops(); ++i) {
                bool isConstant = true;
                ExprSymbolSet vars = Expression(newLhs.op(i)).getVariables();
                for (const ExprSymbol &var: vars) {
                    if (params.find(var) == params.end()) {
                        isConstant = false;
                        break;
                    }
                }
                if (isConstant) {
                    newRhs -= newLhs.op(i);
                }
            }
        } else {
            ExprSymbolSet vars = Expression(newLhs).getVariables();
            bool isConstant = true;
            for (const ExprSymbol &var: vars) {
                if (params.find(var) == params.end()) {
                    isConstant = false;
                    break;
                }
            }
            if (isConstant) {
                newRhs -= newLhs;
            }
        }
        //other cases (mul, pow, sym) should not include numerical constants (only numerical coefficients)

        newLhs += newRhs;
        return replaceLhsRhs(rel, newLhs, newRhs);
    }

    Expression negateLessEqInequality(const Expression &relLessEq) {
        assert(isInequality(relLessEq));
        assert(relLessEq.info(GiNaC::info_flags::relation_less_or_equal));
        return (-relLessEq.lhs()) <= (-relLessEq.rhs())-1;
    }

    option<bool> checkTrivial(const Expression &rel) {
        using namespace GiNaC;
        assert(isRelation(rel));

        Expression diff = (rel.lhs() - rel.rhs()).expand();

        if (diff.isRationalConstant()) {
            relational relZero = ex_to<relational>(replaceLhsRhs(rel, diff, Expression(0)));
            // cast to relZero to bool to evaluate it
            if (relZero) {
                return true;
            } else {
                return false;
            }
        }

        return {};
    }

    bool isTriviallyTrue(const Expression &rel) {
        auto optTrivial = checkTrivial(rel);
        return (optTrivial && optTrivial.get());
    }
}
