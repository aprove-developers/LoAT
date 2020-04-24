#include "z3.hpp"

#include <string>

namespace sat {

Z3::Z3(): solver(z3::solver(ctx)) { }

void Z3::add(const PropExpr &e) {
    solver.add(convert(e));
    z3::params params(ctx);
    params.set(":timeout", Config::Smt::DefaultTimeout);
    solver.set(params);
}

SatResult Z3::check() {
    switch (solver.check()) {
    case z3::check_result::sat: return SatResult::Sat;
    case z3::check_result::unsat: return SatResult::Unsat;
    case z3::check_result::unknown: return SatResult::Unknown;
    }
}

Model Z3::model() const {
    std::map<uint, bool> res;
    const z3::model &m = solver.get_model();
    for (const auto &p: vars) {
        const z3::expr &b = m.eval(p.second);
        if (b.is_true()) {
            res[p.first] = true;
        } else if (b.is_false()) {
            res[p.first] = false;
        }
    }
    return Model(res);
}

void Z3::setTimeout(const uint timeout) {
    z3::params params(ctx);
    params.set(":timeout", timeout);
    solver.set(params);
}

void Z3::push() {
    solver.push();
}

void Z3::pop() {
    solver.pop();
}

std::set<std::set<int>> Z3::cnf(const PropExpr &e) {
    z3::tactic t(ctx, "tseitin-cnf");
    z3::solver s = t.mk_solver();
    const z3::expr &converted = convert(e);
    s.add(converted);
    s.check();
    std::set<std::set<int>> res;
    for (const z3::expr &e: s.assertions()) {
        if (e.is_and()) {
            for (uint i = 0, end = e.num_args(); i < end; ++i) {
                res.insert(convert(e.arg(i)));
            }
        } else {
            res.insert(convert(e));
        }
    }
    return res;
}

z3::expr Z3::convert(const PropExpr &e) {
    if (e->lit()) {
        int lit = e->lit().get();
        uint var = lit >= 0 ? lit : -lit;
        if (vars.count(var) == 0) {
            std::string name = "x" + std::to_string(var);
            vars.emplace(var, ctx.bool_const(name.c_str()));
            varNames.emplace(name, var);
        }
        return lit >= 0 ? vars.at(var) : !vars.at(var);
    } else {
        const PropExprSet &children = e->getChildren();
        z3::expr_vector res(ctx);
        for (const PropExpr &c: children) {
            res.push_back(convert(c));
        }
        return e->isAnd() ? z3::mk_and(res) : z3::mk_or(res);
    }
}

std::set<int> Z3::convert(const z3::expr &e) {
    std::set<int> res;
    if (e.is_false()) {
        return res;
    }
    assert(e.is_or() || e.is_not() || e.is_const());
    if (e.is_const()) {
        res.insert(varNames[e.to_string()]);
    } else if (e.is_not()) {
        res.insert(-varNames[e.arg(0).to_string()]);
    } else {
        for (uint i = 0, size = e.num_args(); i < size; ++i) {
            const z3::expr &child = e.arg(i);
            if (child.is_const()) {
                res.insert(varNames[child.to_string()]);
            } else if (child.is_not()) {
                assert(child.arg(0).is_const());
                res.insert(-varNames[child.arg(0).to_string()]);
            } else {
                assert(false);
            }
        }
    }
    return res;
}

}
