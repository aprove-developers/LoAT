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

#include "z3toolbox.h"

#include "z3solver.h"
#include "z3context.h"
#include "expr/expression.h"
#include "debug.h"

using namespace std;


z3::expr Z3Toolbox::concat(Z3Context &context, const std::vector<z3::expr> &list, ConcatOperator op) {
    z3::expr res = context.bool_val(op == ConcatAnd);
    for (unsigned int i=0; i < list.size(); ++i) {
        if (i == 0) {
            res = list[i];
        } else {
            res = (op == ConcatAnd) ? (res && list[i]) : (res || list[i]);
        }
    }
    return res;
}


Expression Z3Toolbox::getRealFromModel(const z3::model &model, const z3::expr &symbol){
    int num,denom;
    Z3_get_numeral_int(model.ctx(),Z3_get_numerator(model.ctx(),model.eval(symbol,true)),&num);
    Z3_get_numeral_int(model.ctx(),Z3_get_denominator(model.ctx(),model.eval(symbol,true)),&denom);
    assert(denom != 0);

    Expression res(num);
    res = res / denom;
    return res;
}


z3::check_result Z3Toolbox::checkAll(const std::vector<Expression> &list) {
    Z3Context context;
    return checkAll(list, context);
}


z3::check_result Z3Toolbox::checkAll(const vector<Expression> &list, Z3Context &context, z3::model *model) {
    vector<z3::expr> exprvec;
    for (const Expression &expr : list) {
        exprvec.push_back(expr.toZ3(context));
    }
    z3::expr target = concat(context, exprvec, ConcatAnd);

    Z3Solver solver(context);
    solver.add(target);
    z3::check_result z3res = solver.check();
    debugZ3(solver,z3res,"checkAll");

    if (z3res == z3::sat && model) {
        *model = solver.get_model();
    }

    return z3res;
}


z3::check_result Z3Toolbox::checkAllApproximate(const std::vector<Expression> &list) {
    Z3Context context;
    bool useReals = true;

    vector<z3::expr> exprvec;
    for (const Expression &expr : list) {
        // Skip exponentials, z3 cannot handle them well (ok as this is only an approximate check)
        if (expr.isPolynomial()) {
            exprvec.push_back(expr.toZ3(context, useReals));
        }
    }
    z3::expr target = concat(context, exprvec, ConcatAnd);

    Z3Solver solver(context);
    solver.add(target);
    z3::check_result z3res = solver.check();
    debugZ3(solver,z3res,"checkAllApproximate");
    return z3res;
}


bool Z3Toolbox::isValidImplication(const vector<Expression> &lhs, const Expression &rhs) {
    using namespace z3; //for z3::implies, due to a z3 bug
    Z3Context context;

    //rephrase "forall vars: lhs -> rhs" to "not exist vars: (not rhs) and lhs" to avoid universal quantification
    z3::expr rhsExpr = rhs.toZ3(context);
    vector<z3::expr> lhsList;
    for (const Expression &ex : lhs) {
        lhsList.push_back(ex.toZ3(context));
    }

    Z3Solver solver(context);
    solver.add(!rhsExpr && concat(context, lhsList, ConcatAnd));
    return solver.check() == z3::unsat; //must be unsat to prove the original implication
}
