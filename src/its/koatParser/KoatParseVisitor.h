#pragma once


#include "antlr4-runtime.h"
#include "KoatVisitor.h"
#include "../itsproblem.hpp"
#include "../../expr/boolexpr.hpp"
#include "../../util/exceptions.hpp"

/**
 * This class provides an empty implementation of KoatVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  KoatParseVisitor : public KoatVisitor {

    EXCEPTION(ParseError, CustomException);

    ITSProblem its;
    std::map<std::string, LocationIdx> locations;
    std::set<std::string> vars;
    std::vector<Var> programVars;

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
        LocationIdx lhsLoc = visit(ctx->lhs());
        Expr cost = visit(ctx->to());
        std::vector<RuleRhs> rhss = visit(ctx->com());
        BoolExpr cond = True;
        if (ctx->cond()) {
            cond = visit(ctx->cond());
        }
        RuleLhs lhs(lhsLoc, cond, cost);
        its.addRule(Rule(lhs, rhss));
        return {};
    }

    virtual antlrcpp::Any visitLhs(KoatParser::LhsContext *ctx) override {
        static bool initVars = true;
        if (initVars) {
            for (const auto& c: ctx->var()) {
                programVars.push_back(its.addFreshVariable(c->getText()));
            }
            for (const auto& name: vars) {
                if (!its.getVar(name)) {
                    its.addFreshTemporaryVariable(name);
                }
            }
            initVars = false;
        } else {
            unsigned sz = programVars.size();
            if (sz != ctx->var().size()) {
                throw ParseError("wrong arity: " + ctx->getText());
            }
            for (unsigned i = 0; i < sz; ++i) {
                if (programVars[i].get_name() != ctx->var(i)->getText()) {
                    throw ParseError("invalid arguments: " + ctx->getText());
                }
            }
        }
        return visit(ctx->fs());
    }

    virtual antlrcpp::Any visitCom(KoatParser::ComContext *ctx) override {
        std::vector<RuleRhs> rhss;
        for (const auto &rhs: ctx->rhs()) {
            rhss.push_back(visitRhs(rhs));
        }
        return rhss;
    }

    virtual antlrcpp::Any visitRhs(KoatParser::RhsContext *ctx) override {
        const auto expr = ctx->expr();
        unsigned sz = expr.size();
        Subs up;
        for (unsigned i = 0; i < sz; ++i) {
            up.put(programVars[i], visit(expr[i]));
        }
        LocationIdx loc = visit(ctx->fs());
        return RuleRhs(loc, up);
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
            case Exp: return arg1 ^ arg2;
            }
        }
        throw ParseError("failed to parse expression " + ctx->getText());
    }

    virtual antlrcpp::Any visitBinop(KoatParser::BinopContext *ctx) override {
        if (ctx->EXP()) {
            return Exp;
        } else if (ctx->TIMES()) {
            return Times;
        } else if (ctx->PLUS()) {
            return Plus;
        } else if (ctx->MINUS()) {
            return Minus;
        } else {
            throw ParseError("unknown binary operator: " + ctx->getText());
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
        throw ParseError("failed to parse formula " + ctx->getText());
    }

    virtual antlrcpp::Any visitBoolop(KoatParser::BoolopContext *ctx) override {
        if (ctx->AND()) {
            return ConcatAnd;
        } else if (ctx->OR()) {
            return ConcatOr;
        } else {
            throw ParseError("unknown boolean operator: " + ctx->getText());
        }
    }

    virtual antlrcpp::Any visitLit(KoatParser::LitContext *ctx) override {
        const auto &children = ctx->children;
        if (children.size() != 3) {
            throw ParseError("expected relation: " + ctx->getText());
        }
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
        } else if (ctx->GT()) {
            return Rel::gt;
        } else if (ctx->NEQ()) {
            return Rel::neq;
        } else {
            throw ParseError("unknown relation: " + ctx->getText());
        }
    }

};

