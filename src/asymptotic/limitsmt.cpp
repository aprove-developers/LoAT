#include "limitsmt.hpp"

#include "../smt/smt.hpp"
#include "../smt/smtfactory.hpp"
#include "inftyexpression.hpp"
#include "../config.hpp"
#include "../util/timeout.hpp"

using namespace std;


/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies that
 * lim_{n->\infty} p is a positive constant
 */
static BoolExpr posConstraint(const map<int, Expression>& coefficients) {
    BoolExpr conjunction = True;
    for (pair<int, Expression> p : coefficients) {
        int degree = p.first;
        Expression c = p.second;
        if (degree > 0) {
            conjunction = conjunction & (c == 0);
        } else {
            conjunction = conjunction & (c > 0);
        }
    }
    return conjunction;
}

/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies that
 * lim_{n->\infty} p is a negative constant
 */
static BoolExpr negConstraint(const map<int, Expression>& coefficients) {
    BoolExpr conjunction = True;
    for (pair<int, Expression> p : coefficients) {
        int degree = p.first;
        Expression c = p.second;
        if (degree > 0) {
            conjunction = conjunction & (c == 0);
        } else {
            conjunction = conjunction & (c < 0);
        }
    }
    return conjunction;
}

/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies
 * lim_{n->\infty} p = -\infty
 */
static BoolExpr negInfConstraint(const map<int, Expression>& coefficients) {
    int maxDegree = 0;
    for (pair<int, Expression> p: coefficients) {
        maxDegree = p.first > maxDegree ? p.first : maxDegree;
    }
    BoolExpr disjunction = False;
    for (int i = 1; i <= maxDegree; i++) {
        BoolExpr conjunction = True;
        for (pair<int, Expression> p: coefficients) {
            int degree = p.first;
            Expression c = p.second;
            if (degree > i) {
                conjunction = conjunction & (c == 0);
            } else if (degree == i) {
                conjunction = conjunction & (c < 0);
            }
        }
        disjunction = disjunction | conjunction;
    }
    return disjunction;
}

/**
 * Given the (abstract) coefficients of a univariate polynomial p in n (where the key is the
 * degree of the respective monomial), builds an expression which implies
 * lim_{n->\infty} p = \infty
 */
static BoolExpr posInfConstraint(const map<int, Expression>& coefficients) {
    int maxDegree = 0;
    for (pair<int, Expression> p: coefficients) {
        maxDegree = p.first > maxDegree ? p.first : maxDegree;
    }
    BoolExpr disjunction = False;
    for (int i = 1; i <= maxDegree; i++) {
        BoolExpr conjunction = True;
        for (pair<int, Expression> p: coefficients) {
            int degree = p.first;
            Expression c = p.second;
            if (degree > i) {
                conjunction = conjunction & (c == 0);
            } else if (degree == i) {
                conjunction = conjunction & (c > 0);
            }
        }
        disjunction = disjunction | conjunction;
    }
    return disjunction;
}

/**
 * @return the (abstract) coefficients of 'n' in 'ex', where the key is the degree of the respective monomial
 */
static map<int, Expression> getCoefficients(const Expression &ex, const ExprSymbol &n) {
    int maxDegree = ex.degree(n);
    map<int, Expression> coefficients;
    for (int i = 0; i <= maxDegree; i++) {
        coefficients.emplace(i, ex.coeff(n, i));
    }
    return coefficients;
}

option<GiNaC::exmap> LimitSmtEncoding::applyEncoding(const LimitProblem &currentLP, const Expression &cost,
                                                     VarMan &varMan, Complexity currentRes, uint timeout)
{
    // initialize z3
    unique_ptr<Smt> solver = SmtFactory::modelBuildingSolver(Smt::chooseLogic<UpdateMap>({currentLP.getQuery()}, {}), varMan, timeout);

    // the parameter of the desired family of solutions
    ExprSymbol n = currentLP.getN();

    // get all relevant variables
    ExprSymbolSet vars = currentLP.getVariables();

    // create linear templates for all variables
    GiNaC::exmap templateSubs;
    ExprSymbolMap<ExprSymbol> varCoeff, varCoeff0;
    for (const ExprSymbol &var : vars) {
        ExprSymbol c0 = varMan.getFreshUntrackedSymbol(var.get_name() + "_0", Expression::Int);
        ExprSymbol c = varMan.getFreshUntrackedSymbol(var.get_name() + "_c", Expression::Int);
        varCoeff.emplace(var, c);
        varCoeff0.emplace(var, c0);
        templateSubs[var] = c0 + (n * c);
    }

    // replace variables in the cost function with their linear templates
    Expression templateCost = cost.subs(templateSubs).expand();

    // if the cost function is a constant, then we are bound to fail
    Complexity maxPossibleFiniteRes = templateCost.isPolynomial() ?
            Complexity::Poly(templateCost.degree(n)) :
            Complexity::NestedExp;
    if (maxPossibleFiniteRes == Complexity::Const) {
        return {};
    }

    // encode every entry of the limit problem
    for (auto it = currentLP.cbegin(); it != currentLP.cend(); ++it) {
        // replace variables with their linear templates
        Expression ex = it->subs(templateSubs).expand();
        map<int, Expression> coefficients = getCoefficients(ex, n);
        Direction direction = it->getDirection();
        // add the required constraints (depending on the direction-label from the limit problem)
        if (direction == POS) {
            BoolExpr disjunction = posConstraint(coefficients) | posInfConstraint(coefficients);
            solver->add(disjunction);
        } else if (direction == POS_CONS) {
            solver->add(posConstraint(coefficients));
        } else if (direction == POS_INF) {
            solver->add(posInfConstraint(coefficients));
        } else if (direction == NEG_CONS) {
            solver->add(negConstraint(coefficients));
        } else if (direction == NEG_INF) {
            solver->add(negInfConstraint(coefficients));
        }
    }

    // auxiliary function that checks satisfiability wrt. the current state of the solver
    auto checkSolver = [&]() -> bool {
        Smt::Result res = solver->check();
        return res == Smt::Sat;
    };

    // remember the current state for backtracking before trying several variations
    solver->push();

    // first fix that all program variables have to be constants
    // a model witnesses unbounded complexity
    for (const ExprSymbol &var : vars) {
        if (!varMan.isTempVar(var)) {
            solver->add(varCoeff.at(var) == 0);
        }
    }

    if (!checkSolver()) {
        if (maxPossibleFiniteRes <= currentRes) {
            return {};
        }
        // we failed to find a model -- drop all non-mandatory constraints
        solver->pop();
        if (maxPossibleFiniteRes.getType() == Complexity::CpxPolynomial && maxPossibleFiniteRes.getPolynomialDegree().isInteger()) {
            int maxPossibleDegree = maxPossibleFiniteRes.getPolynomialDegree().asInteger();
            // try to find a witness for polynomial complexity with degree maxDeg,...,1
            map<int, Expression> coefficients = getCoefficients(templateCost, n);
            for (int i = maxPossibleDegree; i > 0 && Complexity::Poly(i) > currentRes; i--) {
                Expression c = coefficients.find(i)->second;
                // remember the current state for backtracking
                solver->push();
                solver->add(c > 0);
                if (checkSolver()) {
                    break;
                } else if (i == 1 || Complexity::Poly(i - 1) <= currentRes) {
                    // we even failed to prove the minimal requested bound -- give up
                    return {};
                } else {
                    // remove all non-mandatory constraints and retry with degree i-1
                    solver->pop();
                }
            }
        } else if (!checkSolver()) {
            return {};
        }
    }

    // we found a model -- create the corresponding solution of the limit problem
    GiNaC::exmap smtSubs;
    ExprSymbolMap<GiNaC::numeric> model = solver->model();
    for (const ExprSymbol &var : vars) {
        auto c0 = model.find(varCoeff0.at(var));
        Expression c = model.at(varCoeff.at(var));
        smtSubs[var] = c0 == model.end() ? (c * n) : (c0->second + c * n);
    }

    return {smtSubs};
}
