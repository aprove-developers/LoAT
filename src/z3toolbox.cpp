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

#include "timing.h"
#include "expression.h"
#include "itrs/recursiongraph.h"

#include "debug.h"

using namespace std;


/* ############################## *
 * ### Context implementation ### *
 * ############################## */

z3::expr Z3VariableContext::getVariable(string name, VariableType type) {
    auto it = variables.find(name);
    //create new variable
    if (it == variables.end()) {
        z3::expr res = (type == Integer) ? int_const(name.c_str()) : real_const(name.c_str());
        variables.emplace(name,res);
        basenameCount[name] = 1;
        return res;
    }
    //return existing variable if type matches
    if (isTypeEqual(it->second,type))
        return it->second;

    //name already in use for different type
    return getFreshVariable(name,type);
}

z3::expr Z3VariableContext::getFreshVariable(string name, VariableType type, string *newnameptr) {
    //if name is still free, keep it
    if (variables.find(name) == variables.end())
        return getVariable(name,type);

    string newname;
    do {
        int cnt = basenameCount[name]++;
        string suffix = to_string(cnt);
        newname = name + "_" + suffix;
    } while (variables.find(newname) != variables.end());

    if (newnameptr) *newnameptr = newname;

    return getVariable(newname,type);
}

bool Z3VariableContext::hasVariable(string name, VariableType type) const {
    auto it = variables.find(name);
    return it != variables.end() && isTypeEqual(it->second,type);
}

bool Z3VariableContext::isTypeEqual(const z3::expr &expr, VariableType type) const {
    const z3::sort sort = expr.get_sort();
    return ((type == Integer && sort.is_int()) || (type == Real && sort.is_real()));
}



/* ############################## *
 * ###   Z3Toolbox  methods   ### *
 * ############################## */

z3::expr Z3Toolbox::concatExpressions(Z3VariableContext &context, const std::vector<z3::expr> &list, ConcatOperator op) {
    z3::expr res = context.bool_val(op == ConcatAnd);
    for (int i=0; i < list.size(); ++i) {
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


z3::check_result Z3Toolbox::checkExpressionsSAT(const std::vector<Expression> &list) {
    Z3VariableContext context;
    return checkExpressionsSAT(list,context);
}


z3::check_result Z3Toolbox::checkExpressionsSAT(const vector<Expression> &list, Z3VariableContext &context, z3::model *model) {
    vector<z3::expr> exprvec;
    for (const Expression &expr : list) {
        exprvec.push_back(expr.toZ3(context));
    }
    z3::expr target = concatExpressions(context,exprvec,ConcatAnd);

    Z3Solver solver(context);
    z3::params params(context);
    params.set(":timeout", Z3_CHECK_TIMEOUT);
    solver.set(params);
    solver.add(target);
    z3::check_result z3res = solver.check();
    debugZ3(solver,z3res,"checkExprSAT");

    if (z3res == z3::sat && model) *model = solver.get_model();
    return z3res;
}


z3::check_result Z3Toolbox::checkExpressionsSATapproximate(const std::vector<Expression> &list) {
    Z3VariableContext context;
    vector<z3::expr> exprvec;
    for (const Expression &expr : list) {
        exprvec.push_back(expr.toZ3(context,false,true));
    }
    z3::expr target = concatExpressions(context,exprvec,ConcatAnd);

    Z3Solver solver(context);
    solver.add(target);
    z3::check_result z3res = solver.check();
    debugZ3(solver,z3res,"checkExprSATapprox");
    return z3res;
}


bool Z3Toolbox::checkTautologicImplication(const vector<Expression> &lhs, const Expression &rhs) {
    using namespace z3; //for z3::implies, due to a z3 bug
    Z3VariableContext context;

    //rephrase "forall vars: lhs -> rhs" to "not exist vars: (not rhs) and lhs" to avoid all-quantor
    z3::expr rhsExpr = rhs.toZ3(context);
    vector<z3::expr> lhsList;
    for (const Expression &ex : lhs) lhsList.push_back(ex.toZ3(context));

    Z3Solver solver(context);
    solver.add(!rhsExpr && concatExpressions(context,lhsList,ConcatAnd));
    return solver.check() == z3::unsat; //must be unsat to prove the original implication
}


bool Z3Toolbox::checkTautologicImplication(const vector<Expression> &lhs, const vector<vector<Expression>> &rhs) {
    using namespace z3; //for z3::implies, due to a z3 bug
    Z3VariableContext context;

    //rephrase "forall vars: lhs -> rhs" to "not exist vars: (not rhs) and lhs" to avoid all-quantor
    std::vector<z3::expr> rhsList;
    for (const std::vector<Expression> &conjunction : rhs) {
        std::vector<z3::expr> z3conjunction;
        for (const Expression &ex : conjunction) {
            z3conjunction.push_back(ex.toZ3(context));
        }

        rhsList.push_back(!concatExpressions(context, z3conjunction, ConcatAnd));
    }

    vector<z3::expr> lhsList;
    for (const Expression &ex : lhs) {
        lhsList.push_back(ex.toZ3(context));
    }

    Z3Solver solver(context);
    solver.add(concatExpressions(context, rhsList, ConcatAnd) && concatExpressions(context, lhsList, ConcatAnd));
    return solver.check() == z3::unsat; //must be unsat to prove the original implication
}
