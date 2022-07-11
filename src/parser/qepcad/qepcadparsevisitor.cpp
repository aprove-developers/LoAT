#include "qepcadparsevisitor.h"
#include "../../expr/expression.hpp"

#include "qepcadLexer.h"
#include "qepcadParser.h"

antlrcpp::Any QepcadParseVisitor::visitMain(qepcadParser::MainContext *ctx) {
  return visitFormula(ctx->formula());
}

antlrcpp::Any QepcadParseVisitor::visitExpr(qepcadParser::ExprContext *ctx) {
  if (ctx->VAR()) {
      return Expr(varMan.getVar(ctx->getText()).get());
  } else if (ctx->INT()) {
      return Expr(stoi(ctx->getText()));
  } else if (ctx->EXP()) {
      const Expr arg1 = visit(ctx->expr(0));
      const Expr arg2 = visit(ctx->expr(1));
      return arg1 ^ arg2;
  } else if (ctx->binop()) {
      const BinOp binop = visit(ctx->binop());
      const Expr arg1 = visit(ctx->expr(0));
      const Expr arg2 = visit(ctx->expr(1));
      switch (binop) {
      case Minus: return arg1 - arg2;
      case Plus: return arg1 + arg2;
      }
  } else if (ctx->expr().size() == 2) {
      const Expr arg1 = visit(ctx->expr(0));
      const Expr arg2 = visit(ctx->expr(1));
      return arg1 * arg2;
  }
  throw ParseError("failed to parse qepcad expression: " + ctx->getText());
}

antlrcpp::Any QepcadParseVisitor::visitBinop(qepcadParser::BinopContext *ctx) {
    if (ctx->MINUS()) return Minus;
    else if (ctx->PLUS()) return Plus;
    else throw ParseError("failed to parse qepcad operator: " + ctx->getText());
}

antlrcpp::Any QepcadParseVisitor::visitFormula(qepcadParser::FormulaContext *ctx) {
  if (ctx->lit()) {
      return buildLit(visit(ctx->lit()));
  } else if (ctx->BTRUE()) {
      return True;
  } else if (ctx->BFALSE()) {
      return False;
  } else if (ctx->boolop()) {
      ConcatOperator op = visit(ctx->boolop());
      std::vector<BoolExpr> args;
      for (auto const &f: ctx->formula()) {
          args.push_back(visit(f));
      }
      switch (op) {
      case ConcatAnd: return buildAnd(args);
      case ConcatOr: return buildOr(args);
      }
  }
  throw ParseError("failed to parse qepcad formula: " + ctx->getText());
}

antlrcpp::Any QepcadParseVisitor::visitLit(qepcadParser::LitContext *ctx) {
    Expr arg1 = visit(ctx->expr(0));
    Rel::RelOp op = visit(ctx->relop());
    Expr arg2 = visit(ctx->expr(1));
    return Rel(arg1, op, arg2);
}

antlrcpp::Any QepcadParseVisitor::visitBoolop(qepcadParser::BoolopContext *ctx) {
    if (ctx->AND()) {
        return ConcatAnd;
    } else if (ctx->OR()) {
        return ConcatOr;
    } else {
        throw ParseError("failed to parse boolean qepcad operator: " + ctx->getText());
    }
}

antlrcpp::Any QepcadParseVisitor::visitRelop(qepcadParser::RelopContext *ctx) {
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
        throw ParseError("failed to parse qepcad relation: " + ctx->getText());
    }
}

QepcadParseVisitor::QepcadParseVisitor(VariableManager &varMan): varMan(varMan) {}

BoolExpr QepcadParseVisitor::parse(std::string str, VariableManager &varMan) {
    antlr4::ANTLRInputStream input(str);
    qepcadLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    qepcadParser parser(&tokens);
    parser.setBuildParseTree(true);
    QepcadParseVisitor vis(varMan);
    auto ctx = parser.main();
    if (parser.getNumberOfSyntaxErrors() > 0) {
        throw ParseError("parsing qepcad formula failed");
    } else {
        return vis.visit(ctx);
    }
}
