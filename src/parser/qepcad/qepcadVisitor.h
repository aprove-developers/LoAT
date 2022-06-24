
// Generated from qepcad.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "qepcadParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by qepcadParser.
 */
class  qepcadVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by qepcadParser.
   */
    virtual antlrcpp::Any visitMain(qepcadParser::MainContext *context) = 0;

    virtual antlrcpp::Any visitExpr(qepcadParser::ExprContext *context) = 0;

    virtual antlrcpp::Any visitBinop(qepcadParser::BinopContext *context) = 0;

    virtual antlrcpp::Any visitFormula(qepcadParser::FormulaContext *context) = 0;

    virtual antlrcpp::Any visitLit(qepcadParser::LitContext *context) = 0;

    virtual antlrcpp::Any visitBoolop(qepcadParser::BoolopContext *context) = 0;

    virtual antlrcpp::Any visitRelop(qepcadParser::RelopContext *context) = 0;


};

