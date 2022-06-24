
// Generated from qepcad.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "qepcadParser.h"


/**
 * This interface defines an abstract listener for a parse tree produced by qepcadParser.
 */
class  qepcadListener : public antlr4::tree::ParseTreeListener {
public:

  virtual void enterMain(qepcadParser::MainContext *ctx) = 0;
  virtual void exitMain(qepcadParser::MainContext *ctx) = 0;

  virtual void enterExpr(qepcadParser::ExprContext *ctx) = 0;
  virtual void exitExpr(qepcadParser::ExprContext *ctx) = 0;

  virtual void enterBinop(qepcadParser::BinopContext *ctx) = 0;
  virtual void exitBinop(qepcadParser::BinopContext *ctx) = 0;

  virtual void enterFormula(qepcadParser::FormulaContext *ctx) = 0;
  virtual void exitFormula(qepcadParser::FormulaContext *ctx) = 0;

  virtual void enterLit(qepcadParser::LitContext *ctx) = 0;
  virtual void exitLit(qepcadParser::LitContext *ctx) = 0;

  virtual void enterBoolop(qepcadParser::BoolopContext *ctx) = 0;
  virtual void exitBoolop(qepcadParser::BoolopContext *ctx) = 0;

  virtual void enterRelop(qepcadParser::RelopContext *ctx) = 0;
  virtual void exitRelop(qepcadParser::RelopContext *ctx) = 0;


};

