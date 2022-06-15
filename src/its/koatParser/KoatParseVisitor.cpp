#include "KoatParseVisitor.h"

antlrcpp::Any KoatParseVisitor::visitMain(KoatParser::MainContext *ctx) {
    visitChildren(ctx);
    return its;
}

antlrcpp::Any KoatParseVisitor::visitGoal(KoatParser::GoalContext *ctx) {
    return {};
}

antlrcpp::Any KoatParseVisitor::visitStart(KoatParser::StartContext *ctx) {
    its.setInitialLocation(visit(ctx->fs()));
    return {};
}

antlrcpp::Any KoatParseVisitor::visitVardecl(KoatParser::VardeclContext *ctx) {
    for (const auto &c: ctx->ID()) {
        vars.insert(c->getText());
    }
    return {};
}

antlrcpp::Any KoatParseVisitor::visitTranss(KoatParser::TranssContext *ctx) {
    visitChildren(ctx);
    return {};
}

antlrcpp::Any KoatParseVisitor::visitVar(KoatParser::VarContext *ctx) {
    return its.getVar(ctx->getText()).get();
}

antlrcpp::Any KoatParseVisitor::visitFs(KoatParser::FsContext *ctx) {
    return loc(ctx->getText());
}

antlrcpp::Any KoatParseVisitor::visitTrans(KoatParser::TransContext *ctx) {
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

antlrcpp::Any KoatParseVisitor::visitLhs(KoatParser::LhsContext *ctx) {
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

antlrcpp::Any KoatParseVisitor::visitCom(KoatParser::ComContext *ctx) {
    std::vector<RuleRhs> rhss;
    for (const auto &rhs: ctx->rhs()) {
        rhss.push_back(visitRhs(rhs));
    }
    return rhss;
}

antlrcpp::Any KoatParseVisitor::visitRhs(KoatParser::RhsContext *ctx) {
    const auto expr = ctx->expr();
    unsigned sz = expr.size();
    Subs up;
    for (unsigned i = 0; i < sz; ++i) {
        up.put(programVars[i], visit(expr[i]));
    }
    LocationIdx loc = visit(ctx->fs());
    return RuleRhs(loc, up);
}

antlrcpp::Any KoatParseVisitor::visitTo(KoatParser::ToContext *ctx) {
    if (ctx->lb()) {
        return visit(ctx->lb());
    } else {
        return Expr(1);
    }
}

antlrcpp::Any KoatParseVisitor::visitLb(KoatParser::LbContext *ctx) {
    return visit(ctx->expr());
}

antlrcpp::Any KoatParseVisitor::visitUb(KoatParser::UbContext *ctx) {
    return {};
}

antlrcpp::Any KoatParseVisitor::visitCond(KoatParser::CondContext *ctx) {
    return visit(ctx->formula());
}

antlrcpp::Any KoatParseVisitor::visitExpr(KoatParser::ExprContext *ctx) {
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

antlrcpp::Any KoatParseVisitor::visitBinop(KoatParser::BinopContext *ctx) {
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

antlrcpp::Any KoatParseVisitor::visitFormula(KoatParser::FormulaContext *ctx) {
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

antlrcpp::Any KoatParseVisitor::visitBoolop(KoatParser::BoolopContext *ctx) {
    if (ctx->AND()) {
        return ConcatAnd;
    } else if (ctx->OR()) {
        return ConcatOr;
    } else {
        throw ParseError("unknown boolean operator: " + ctx->getText());
    }
}

antlrcpp::Any KoatParseVisitor::visitLit(KoatParser::LitContext *ctx) {
    const auto &children = ctx->children;
    if (children.size() != 3) {
        throw ParseError("expected relation: " + ctx->getText());
    }
    Expr arg1 = visit(ctx->expr(0));
    Rel::RelOp op = visit(children[1]);
    Expr arg2 = visit(ctx->expr(1));
    return Rel(arg1, op, arg2);
}

antlrcpp::Any KoatParseVisitor::visitRelop(KoatParser::RelopContext *ctx) {
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
