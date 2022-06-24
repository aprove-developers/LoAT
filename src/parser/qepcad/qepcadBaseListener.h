
// Generated from qepcad.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"
#include "qepcadListener.h"


/**
 * This class provides an empty implementation of qepcadListener,
 * which can be extended to create a listener which only needs to handle a subset
 * of the available methods.
 */
class  qepcadBaseListener : public qepcadListener {
public:

  virtual void enterMain(qepcadParser::MainContext * /*ctx*/) override { }
  virtual void exitMain(qepcadParser::MainContext * /*ctx*/) override { }

  virtual void enterExpr(qepcadParser::ExprContext * /*ctx*/) override { }
  virtual void exitExpr(qepcadParser::ExprContext * /*ctx*/) override { }

  virtual void enterBinop(qepcadParser::BinopContext * /*ctx*/) override { }
  virtual void exitBinop(qepcadParser::BinopContext * /*ctx*/) override { }

  virtual void enterFormula(qepcadParser::FormulaContext * /*ctx*/) override { }
  virtual void exitFormula(qepcadParser::FormulaContext * /*ctx*/) override { }

  virtual void enterLit(qepcadParser::LitContext * /*ctx*/) override { }
  virtual void exitLit(qepcadParser::LitContext * /*ctx*/) override { }

  virtual void enterBoolop(qepcadParser::BoolopContext * /*ctx*/) override { }
  virtual void exitBoolop(qepcadParser::BoolopContext * /*ctx*/) override { }

  virtual void enterRelop(qepcadParser::RelopContext * /*ctx*/) override { }
  virtual void exitRelop(qepcadParser::RelopContext * /*ctx*/) override { }


  virtual void enterEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void exitEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void visitTerminal(antlr4::tree::TerminalNode * /*node*/) override { }
  virtual void visitErrorNode(antlr4::tree::ErrorNode * /*node*/) override { }

};

