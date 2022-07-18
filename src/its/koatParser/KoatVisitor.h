
// Generated from Koat.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "KoatParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by KoatParser.
 */
class  KoatVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by KoatParser.
   */
    virtual antlrcpp::Any visitMain(KoatParser::MainContext *context) = 0;

    virtual antlrcpp::Any visitFs(KoatParser::FsContext *context) = 0;

    virtual antlrcpp::Any visitVar(KoatParser::VarContext *context) = 0;

    virtual antlrcpp::Any visitGoal(KoatParser::GoalContext *context) = 0;

    virtual antlrcpp::Any visitStart(KoatParser::StartContext *context) = 0;

    virtual antlrcpp::Any visitVardecl(KoatParser::VardeclContext *context) = 0;

    virtual antlrcpp::Any visitTranss(KoatParser::TranssContext *context) = 0;

    virtual antlrcpp::Any visitTrans(KoatParser::TransContext *context) = 0;

    virtual antlrcpp::Any visitLhs(KoatParser::LhsContext *context) = 0;

    virtual antlrcpp::Any visitCom(KoatParser::ComContext *context) = 0;

    virtual antlrcpp::Any visitRhs(KoatParser::RhsContext *context) = 0;

    virtual antlrcpp::Any visitTo(KoatParser::ToContext *context) = 0;

    virtual antlrcpp::Any visitLb(KoatParser::LbContext *context) = 0;

    virtual antlrcpp::Any visitUb(KoatParser::UbContext *context) = 0;

    virtual antlrcpp::Any visitCond(KoatParser::CondContext *context) = 0;

    virtual antlrcpp::Any visitExpr(KoatParser::ExprContext *context) = 0;

    virtual antlrcpp::Any visitFormula(KoatParser::FormulaContext *context) = 0;

    virtual antlrcpp::Any visitLit(KoatParser::LitContext *context) = 0;

    virtual antlrcpp::Any visitRelop(KoatParser::RelopContext *context) = 0;


};

