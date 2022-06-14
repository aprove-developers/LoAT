
// Generated from Koat.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "KoatVisitor.h"
#include "../itsproblem.hpp"
#include "../../expr/boolexpr.hpp"

/**
 * This class provides an empty implementation of KoatVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  KoatBaseVisitor : public KoatVisitor {

    ITSProblem its;
    std::map<std::string, LocationIdx> locations;
    std::set<std::string> vars;

    enum ArithOp {
        Plus, Minus, Times, Exp
    };

    LocationIdx loc(const std::string &name) {
        auto it = locations.find(name);
        if (it == locations.end()) {
            auto idx = its.addNamedLocation(name);
            locations[name] = idx;
            return idx;
        } else {
            return it->second;
        }
    }

public:

    virtual antlrcpp::Any visitMain(KoatParser::MainContext *ctx) override {
        visitChildren(ctx);
        return its;
    }

    virtual antlrcpp::Any visitGoal(KoatParser::GoalContext *ctx) override {
        return {};
    }

    virtual antlrcpp::Any visitStart(KoatParser::StartContext *ctx) override {
        its.setInitialLocation(visit(ctx->fs()));
        return {};
    }

    virtual antlrcpp::Any visitVardecl(KoatParser::VardeclContext *ctx) override {
        for (const auto &c: ctx->ID()) {
            vars.insert(c->getText());
        }
        return {};
    }

    virtual antlrcpp::Any visitTranss(KoatParser::TranssContext *ctx) override {
        visitChildren(ctx);
        return {};
    }

    virtual antlrcpp::Any visitVar(KoatParser::VarContext *ctx) override {
        return its.getVar(ctx->getText()).get();
    }

    virtual antlrcpp::Any visitFs(KoatParser::FsContext *ctx) override {
        return loc(ctx->getText());
    }

    virtual antlrcpp::Any visitTrans(KoatParser::TransContext *ctx) override {
        std::pair<LocationIdx, std::vector<Var>> lhs = visit(ctx->lhs());
        LocationIdx lhsLoc = lhs.first;
        std::vector<Var> lhsArgs = lhs.second;
        Expr cost = visit(ctx->to());
        std::pair<LocationIdx, std::vector<Expr>> rhs = visit(ctx->rhs());
        LocationIdx rhsLoc = rhs.first;
        std::vector<Expr> rhsArgs = rhs.second;
        assert(lhsArgs.size() == rhsArgs.size());
        unsigned sz = lhsArgs.size();
        Subs up;
        for (unsigned i = 0; i < sz; ++i) {
            if (!rhsArgs[i].equals(lhsArgs[i])) {
                up.put(lhsArgs[i], rhsArgs[i]);
            }
        }
        BoolExpr cond = True;
        if (ctx->cond()) {
            cond = visit(ctx->cond());
        }
        its.addRule(Rule(lhsLoc, cond, cost, rhsLoc, up));
        return {};
    }

    virtual antlrcpp::Any visitLhs(KoatParser::LhsContext *ctx) override {
        static bool initVars = true;
        if (initVars) {
            for (const auto& c: ctx->var()) {
                its.addFreshVariable(c->getText());
            }
            for (const auto& name: vars) {
                if (!its.getVar(name)) {
                    its.addFreshTemporaryVariable(name);
                }
            }
            initVars = false;
        }
        const auto vars = ctx->var();
        unsigned sz = vars.size();
        std::vector<Var> args;
        args.resize(sz);
        for (unsigned i = 0; i < sz; ++i) {
            args[i] = visit(vars[i]);
        }
        LocationIdx loc = visit(ctx->fs());
        return std::pair<LocationIdx, std::vector<Var>>(loc, args);
    }

    virtual antlrcpp::Any visitRhs(KoatParser::RhsContext *ctx) override {
        const auto expr = ctx->expr();
        unsigned sz = expr.size();
        std::vector<Expr> args;
        args.resize(sz);
        for (unsigned i = 0; i < sz; ++i) {
            args[i] = visit(expr[i]);
        }
        LocationIdx loc = visit(ctx->fs());
        return std::pair<LocationIdx, std::vector<Expr>>(loc, args);
    }

    virtual antlrcpp::Any visitTo(KoatParser::ToContext *ctx) override {
        if (ctx->lb()) {
            return visit(ctx->lb());
        } else {
            return Expr(1);
        }
    }

    virtual antlrcpp::Any visitLb(KoatParser::LbContext *ctx) override {
        return visit(ctx->expr());
    }

    virtual antlrcpp::Any visitUb(KoatParser::UbContext *ctx) override {
        return {};
    }

    virtual antlrcpp::Any visitCond(KoatParser::CondContext *ctx) override {
        return visit(ctx->formula());
    }

    virtual antlrcpp::Any visitExpr(KoatParser::ExprContext *ctx) override {
        if (ctx->INT()) {
            return Expr(stoi(ctx->INT()->getText()));
        } else if (ctx->var()) {
            Var var = visit(ctx->var());
            return Expr(var);
        } else if (ctx->LPAR()) {
            return visit(ctx->expr(0));
        } else if (ctx->MINUS()) {
            Expr res = visit(ctx->expr(0));
            return -res;
        } else {
            Expr arg1 = visit(ctx->expr(0));
            ArithOp op = visit(ctx->binop());
            Expr arg2 = visit(ctx->expr(1));
            switch (op) {
            case Plus: return arg1 + arg2;
            case Minus: return arg1 - arg2;
            case Times: return arg1 * arg2;
            case Exp: assert(false); // TODO
            }
        }
        assert(false);
    }

    virtual antlrcpp::Any visitBinop(KoatParser::BinopContext *ctx) override {
        if (ctx->EXP()) {
            return Exp;
        } else if (ctx->TIMES()) {
            return Times;
        } else if (ctx->PLUS()) {
            return Plus;
        } else {
            assert(ctx->MINUS());
            return Minus;
        }
    }

    virtual antlrcpp::Any visitFormula(KoatParser::FormulaContext *ctx) override {
        if (ctx->lit()) {
            return buildLit(visit(ctx->lit()));
        } else if (ctx->LPAR()) {
            return visit(ctx->formula(0));
        } else {
            BoolExpr arg1 = visit(ctx->formula(0));
            ConcatOperator op = visit(ctx->boolop());
            BoolExpr arg2 = visit(ctx->formula(1));
            switch (op) {
            case ConcatAnd: return arg1 & arg2;
            case ConcatOr: return arg1 | arg2;
            }
        }
        assert(false);
    }

    virtual antlrcpp::Any visitBoolop(KoatParser::BoolopContext *ctx) override {
        if (ctx->AND()) {
            return ConcatAnd;
        } else {
            assert(ctx->OR());
            return ConcatOr;
        }
    }

    virtual antlrcpp::Any visitLit(KoatParser::LitContext *ctx) override {
        const auto &children = ctx->children;
        assert(children.size() == 3);
        Expr arg1 = visit(ctx->expr(0));
        Rel::RelOp op = visit(children[1]);
        Expr arg2 = visit(ctx->expr(1));
        return Rel(arg1, op, arg2);
    }

    virtual antlrcpp::Any visitRelop(KoatParser::RelopContext *ctx) override {
        if (ctx->LT()) {
            return Rel::lt;
        } else if (ctx->LEQ()) {
            return Rel::leq;
        } else if (ctx->EQ()) {
            return Rel::eq;
        } else if (ctx->GEQ()) {
            return Rel::geq;
        } else {
            assert(ctx->GT());
            return Rel::gt;
        }
    }

};

