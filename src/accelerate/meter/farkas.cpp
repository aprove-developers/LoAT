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

#include "../../smt/smt.hpp"
#include "../../its/variablemanager.hpp"

using namespace std;


BoolExpr FarkasLemma::apply(
        const vector<Rel> &constraints,
        const vector<Var> &vars,
        const vector<Expr> &coeffs,
        Expr c0,
        int delta,
        VariableManager &varMan,
        const VarSet &params,
        Expr::Type lambdaType)
{
    assert(vars.size() == coeffs.size());

    // List of expressions whose conjunction will be the result
    BoolExpr res = True;

    // Create lambda variables, add the constraint "lambda >= 0"
    vector<Var> lambda;
    VarSet varSet(vars.begin(), vars.end());
    for (const Rel &rel : constraints) {
        assert(rel.isLinear(varSet) && rel.isIneq());
        assert(rel.relOp() == Rel::leq);

        Var var = varMan.getFreshUntrackedSymbol("l", lambdaType);
        lambda.push_back(var);
        res = res & (var >= 0);
    }

    // Create mapping from every variable to its coefficient
    VarMap<Expr> varToCoeff;
    for (unsigned int i=0; i < vars.size(); ++i) {
        varToCoeff.emplace(vars[i], coeffs[i]);
    }

    // Search for additional variables that are not contained in vars, but appear in constraints.
    // This is neccessary, since these variables appear in the A*x part and thus also have to appear in the c*x part.
    // The coefficients of additional variables are simply set to 0 (so they won't occur in the metering function).
    VarSet constraintSymbols;
    for (const Rel &rel : constraints) {
        rel.collectVariables(constraintSymbols);
    }
    for (const Var &sym : constraintSymbols) {
        if (varToCoeff.find(sym) == varToCoeff.end() && std::find(params.begin(), params.end(), sym) == params.end()) {
            varToCoeff.emplace(sym, 0);
        }
    }

    // Build the constraints "lambda^T * A = c^T"
    for (auto varIt : varToCoeff) {
        Expr lambdaA = 0;
        for (unsigned int j=0; j < constraints.size(); ++j) {
            Expr a = constraints[j].lhs().expand().coeff(varIt.first);
            Expr add = lambda[j] * a;
            lambdaA = (j==0) ? add : lambdaA + add; // avoid superflous +0
        }
        res = res & Rel::buildEq(lambdaA, varIt.second);
    }

    // Build the constraints "lambda^T * b + c0 <= delta"
    Expr sum = c0;
    for (unsigned int i=0; i < constraints.size(); ++i) {
        sum = sum + lambda[i] * constraints[i].rhs();
    }
    return res & (sum <= delta);
}

BoolExpr FarkasLemma::apply(
        const vector<Rel> &constraints,
        const vector<Var> &vars,
        const vector<Var> &coeffs,
        Var c0,
        int delta,
        VariableManager &varMan,
        const VarSet &params,
        Expr::Type lambdaType)
{
    std::vector<Expr> theCoeffs;
    for (const Var &x: coeffs) {
        theCoeffs.push_back(x);
    }
    return apply(constraints, vars, theCoeffs, c0, delta, varMan, params, lambdaType);
}

const BoolExpr FarkasLemma::apply(
        const vector<Rel> &premise,
        const vector<Rel> &conclusion,
        const VarSet &vars,
        const VarSet &params,
        VariableManager &varMan,
        Expr::Type lambdaType) {
    BoolExpr res = True;
    std::vector<Rel> normalizedPremise;
    for (const Rel &p: premise) {
        if (p.isLinear(vars) && p.isIneq()) {
            normalizedPremise.push_back(p.toLeq().splitVariableAndConstantAddends(params));
        } else if (p.isLinear(vars) && p.isEq()) {
            normalizedPremise.push_back((p.lhs() <= p.rhs()).splitVariableAndConstantAddends(params));
            normalizedPremise.push_back((p.rhs() <= p.lhs()).splitVariableAndConstantAddends(params));
        }
    }
    vector<Var> varList(vars.begin(), vars.end());
    vector<Rel> splitConclusion;
    for (const Rel &c: conclusion) {
        if (c.isLinear(vars) && c.isIneq()) {
            splitConclusion.push_back(c);
        } else if (c.isLinear(vars) && c.isEq()) {
            splitConclusion.emplace_back(c.lhs() <= c.rhs());
            splitConclusion.emplace_back(c.rhs() <= c.lhs());
        } else {
            assert(false);
        }
    }
    for (const Rel &c: splitConclusion) {
        Rel normalized = c.toLeq().splitVariableAndConstantAddends(params);
        vector<Expr> coefficients;
        for (Var &x : varList) {
            coefficients.push_back(normalized.lhs().coeff(x, 1));
        }
        Expr c0 = -normalized.rhs();
        res = res & FarkasLemma::apply(normalizedPremise, varList, coefficients, c0, 0, varMan, params, lambdaType);
    }
    return res;
}

const BoolExpr FarkasLemma::apply(
        const vector<Rel> &premise,
        const Rel &conclusion,
        const VarSet &vars,
        const VarSet &params,
        VariableManager &varMan,
        Expr::Type lambdaType) {
    const vector<Rel> &conclusions = {conclusion};
    return apply(premise, conclusions, vars, params, varMan, lambdaType);
}
