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

#include "farkas.hpp"

#include "../../expr/relation.hpp"
#include "../../expr/ginactoz3.hpp"
#include "../../smt/smt.hpp"
#include "../../its/variablemanager.hpp"

using namespace std;


BoolExpr FarkasLemma::apply(
        const vector<Expression> &constraints,
        const vector<ExprSymbol> &vars,
        const vector<Expression> &coeffs,
        Expression c0,
        int delta,
        VariableManager &varMan,
        const ExprSymbolSet &params,
        Expression::Type lambdaType)
{
    assert(vars.size() == coeffs.size());

    // List of expressions whose conjunction will be the result
    BoolExpr res = True;

    // Create lambda variables, add the constraint "lambda >= 0"
    vector<ExprSymbol> lambda;
    ExprSymbolSet varSet(vars.begin(), vars.end());
    for (const Expression &ex : constraints) {
        assert(Relation::isLinearInequality(ex, varSet));
        assert(Relation::isLessOrEqual(ex));

        ExprSymbol var = varMan.getFreshUntrackedSymbol("l", lambdaType);
        lambda.push_back(var);
        res = res & (var >= 0);
    }

    // Create mapping from every variable to its coefficient
    ExprSymbolMap<Expression> varToCoeff;
    for (unsigned int i=0; i < vars.size(); ++i) {
        varToCoeff.emplace(vars[i], coeffs[i]);
    }

    // Search for additional variables that are not contained in vars, but appear in constraints.
    // This is neccessary, since these variables appear in the A*x part and thus also have to appear in the c*x part.
    // The coefficients of additional variables are simply set to 0 (so they won't occur in the metering function).
    ExprSymbolSet constraintSymbols;
    for (const Expression &ex : constraints) {
        ex.collectVariables(constraintSymbols);
    }
    for (const ExprSymbol &sym : constraintSymbols) {
        if (varToCoeff.find(sym) == varToCoeff.end() && std::find(params.begin(), params.end(), sym) == params.end()) {
            varToCoeff.emplace(sym, 0);
        }
    }

    // Build the constraints "lambda^T * A = c^T"
    for (auto varIt : varToCoeff) {
        Expression lambdaA = Expression(0);
        for (unsigned int j=0; j < constraints.size(); ++j) {
            Expression a = constraints[j].lhs().expand().coeff(varIt.first);
            Expression add = lambda[j] * a;
            lambdaA = (j==0) ? add : lambdaA + add; // avoid superflous +0
        }
        res = res & (lambdaA == varIt.second);
    }

    // Build the constraints "lambda^T * b + c0 <= delta"
    Expression sum = c0;
    for (unsigned int i=0; i < constraints.size(); ++i) {
        sum = sum + lambda[i] * constraints[i].rhs();
    }
    return res & (sum <= delta);
}

BoolExpr FarkasLemma::apply(
        const vector<Expression> &constraints,
        const vector<ExprSymbol> &vars,
        const vector<ExprSymbol> &coeffs,
        ExprSymbol c0,
        int delta,
        VariableManager &varMan,
        const ExprSymbolSet &params,
        Expression::Type lambdaType)
{
    std::vector<Expression> theCoeffs;
    for (const ExprSymbol &x: coeffs) {
        theCoeffs.push_back(x);
    }
    return apply(constraints, vars, theCoeffs, c0, delta, varMan, params, lambdaType);
}

const BoolExpr FarkasLemma::apply(
        const vector<Expression> &premise,
        const vector<Expression> &conclusion,
        const ExprSymbolSet &vars,
        const ExprSymbolSet &params,
        VariableManager &varMan,
        Expression::Type lambdaType) {
    BoolExpr res = True;
    std::vector<Expression> normalizedPremise;
    for (const Expression &p: premise) {
        if (Relation::isLinearInequality(p, vars)) {
            normalizedPremise.push_back(Relation::splitVariablesAndConstants(Relation::toLessEq(p), params));
        } else if (Relation::isLinearEquality(p, vars)) {
            normalizedPremise.push_back(Relation::splitVariablesAndConstants(p.lhs() <= p.rhs(), params));
            normalizedPremise.push_back(Relation::splitVariablesAndConstants(p.rhs() <= p.lhs(), params));
        }
    }
    vector<ExprSymbol> varList(vars.begin(), vars.end());
    vector<Expression> splitConclusion;
    for (const Expression &c: conclusion) {
        if (Relation::isLinearInequality(c, vars)) {
            splitConclusion.push_back(c);
        } else if (Relation::isLinearEquality(c, vars)) {
            splitConclusion.emplace_back(c.lhs() <= c.rhs());
            splitConclusion.emplace_back(c.rhs() <= c.lhs());
        } else {
            assert(false);
        }
    }
    for (const Expression &c: splitConclusion) {
        Expression normalized = Relation::splitVariablesAndConstants(Relation::toLessEq(c), params);
        vector<Expression> coefficients;
        for (ExprSymbol &x : varList) {
            coefficients.push_back(Expression(normalized.lhs().coeff(x, 1)));
        }
        Expression c0 = -Expression(normalized.rhs());
        res = res & FarkasLemma::apply(normalizedPremise, varList, coefficients, c0, 0, varMan, params, lambdaType);
    }
    return res;
}

const BoolExpr FarkasLemma::apply(
        const vector<Expression> &premise,
        const Expression &conclusion,
        const ExprSymbolSet &vars,
        const ExprSymbolSet &params,
        VariableManager &varMan,
        Expression::Type lambdaType) {
    const vector<Expression> &conclusions = {conclusion};
    return apply(premise, conclusions, vars, params, varMan, lambdaType);
}
