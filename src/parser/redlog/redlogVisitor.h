
// Generated from redlog.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "redlogParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by redlogParser.
 */
class  redlogVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by redlogParser.
   */
    virtual antlrcpp::Any visitMain(redlogParser::MainContext *context) = 0;

    virtual antlrcpp::Any visitExpr(redlogParser::ExprContext *context) = 0;

    virtual antlrcpp::Any visitCaop(redlogParser::CaopContext *context) = 0;

    virtual antlrcpp::Any visitBinop(redlogParser::BinopContext *context) = 0;

    virtual antlrcpp::Any visitFormula(redlogParser::FormulaContext *context) = 0;

    virtual antlrcpp::Any visitLit(redlogParser::LitContext *context) = 0;

    virtual antlrcpp::Any visitBoolop(redlogParser::BoolopContext *context) = 0;

    virtual antlrcpp::Any visitRelop(redlogParser::RelopContext *context) = 0;


};

