
// Generated from qepcad.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "qepcadVisitor.h"


/**
 * This class provides an empty implementation of qepcadVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  qepcadBaseVisitor : public qepcadVisitor {
public:

  virtual antlrcpp::Any visitMain(qepcadParser::MainContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitExpr(qepcadParser::ExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBinop(qepcadParser::BinopContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitFormula(qepcadParser::FormulaContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLit(qepcadParser::LitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBoolop(qepcadParser::BoolopContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitRelop(qepcadParser::RelopContext *ctx) override {
    return visitChildren(ctx);
  }


};

