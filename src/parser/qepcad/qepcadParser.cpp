
// Generated from qepcad.g4 by ANTLR 4.7.2


#include "qepcadListener.h"
#include "qepcadVisitor.h"

#include "qepcadParser.h"


using namespace antlrcpp;
using namespace antlr4;

qepcadParser::qepcadParser(TokenStream *input) : Parser(input) {
  _interpreter = new atn::ParserATNSimulator(this, _atn, _decisionToDFA, _sharedContextCache);
}

qepcadParser::~qepcadParser() {
  delete _interpreter;
}

std::string qepcadParser::getGrammarFileName() const {
  return "qepcad.g4";
}

const std::vector<std::string>& qepcadParser::getRuleNames() const {
  return _ruleNames;
}

dfa::Vocabulary& qepcadParser::getVocabulary() const {
  return _vocabulary;
}


//----------------- MainContext ------------------------------------------------------------------

qepcadParser::MainContext::MainContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

qepcadParser::FormulaContext* qepcadParser::MainContext::formula() {
  return getRuleContext<qepcadParser::FormulaContext>(0);
}


size_t qepcadParser::MainContext::getRuleIndex() const {
  return qepcadParser::RuleMain;
}

void qepcadParser::MainContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterMain(this);
}

void qepcadParser::MainContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitMain(this);
}


antlrcpp::Any qepcadParser::MainContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<qepcadVisitor*>(visitor))
    return parserVisitor->visitMain(this);
  else
    return visitor->visitChildren(this);
}

qepcadParser::MainContext* qepcadParser::main() {
  MainContext *_localctx = _tracker.createInstance<MainContext>(_ctx, getState());
  enterRule(_localctx, 0, qepcadParser::RuleMain);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(14);
    formula(0);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprContext ------------------------------------------------------------------

qepcadParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* qepcadParser::ExprContext::VAR() {
  return getToken(qepcadParser::VAR, 0);
}

tree::TerminalNode* qepcadParser::ExprContext::INT() {
  return getToken(qepcadParser::INT, 0);
}

tree::TerminalNode* qepcadParser::ExprContext::LPAR() {
  return getToken(qepcadParser::LPAR, 0);
}

std::vector<qepcadParser::ExprContext *> qepcadParser::ExprContext::expr() {
  return getRuleContexts<qepcadParser::ExprContext>();
}

qepcadParser::ExprContext* qepcadParser::ExprContext::expr(size_t i) {
  return getRuleContext<qepcadParser::ExprContext>(i);
}

tree::TerminalNode* qepcadParser::ExprContext::EXP() {
  return getToken(qepcadParser::EXP, 0);
}

tree::TerminalNode* qepcadParser::ExprContext::RPAR() {
  return getToken(qepcadParser::RPAR, 0);
}

qepcadParser::BinopContext* qepcadParser::ExprContext::binop() {
  return getRuleContext<qepcadParser::BinopContext>(0);
}


size_t qepcadParser::ExprContext::getRuleIndex() const {
  return qepcadParser::RuleExpr;
}

void qepcadParser::ExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExpr(this);
}

void qepcadParser::ExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExpr(this);
}


antlrcpp::Any qepcadParser::ExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<qepcadVisitor*>(visitor))
    return parserVisitor->visitExpr(this);
  else
    return visitor->visitChildren(this);
}


qepcadParser::ExprContext* qepcadParser::expr() {
   return expr(0);
}

qepcadParser::ExprContext* qepcadParser::expr(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  qepcadParser::ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, parentState);
  qepcadParser::ExprContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 2;
  enterRecursionRule(_localctx, 2, qepcadParser::RuleExpr, precedence);

    

  auto onExit = finally([=] {
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(36);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      setState(17);
      match(qepcadParser::VAR);
      break;
    }

    case 2: {
      setState(18);
      match(qepcadParser::INT);
      break;
    }

    case 3: {
      setState(19);
      match(qepcadParser::LPAR);
      setState(20);
      expr(0);
      setState(21);
      match(qepcadParser::EXP);
      setState(22);
      expr(0);
      setState(23);
      match(qepcadParser::RPAR);
      break;
    }

    case 4: {
      setState(25);
      match(qepcadParser::LPAR);
      setState(26);
      expr(0);
      setState(27);
      expr(0);
      setState(28);
      match(qepcadParser::RPAR);
      break;
    }

    case 5: {
      setState(30);
      match(qepcadParser::LPAR);
      setState(31);
      expr(0);
      setState(32);
      binop();
      setState(33);
      expr(0);
      setState(34);
      match(qepcadParser::RPAR);
      break;
    }

    }
    _ctx->stop = _input->LT(-1);
    setState(49);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 2, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        setState(47);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 1, _ctx)) {
        case 1: {
          _localctx = _tracker.createInstance<ExprContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleExpr);
          setState(38);

          if (!(precpred(_ctx, 5))) throw FailedPredicateException(this, "precpred(_ctx, 5)");
          setState(39);
          match(qepcadParser::EXP);
          setState(40);
          expr(6);
          break;
        }

        case 2: {
          _localctx = _tracker.createInstance<ExprContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleExpr);
          setState(41);

          if (!(precpred(_ctx, 3))) throw FailedPredicateException(this, "precpred(_ctx, 3)");
          setState(42);
          expr(4);
          break;
        }

        case 3: {
          _localctx = _tracker.createInstance<ExprContext>(parentContext, parentState);
          pushNewRecursionContext(_localctx, startState, RuleExpr);
          setState(43);

          if (!(precpred(_ctx, 1))) throw FailedPredicateException(this, "precpred(_ctx, 1)");
          setState(44);
          binop();
          setState(45);
          expr(2);
          break;
        }

        } 
      }
      setState(51);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 2, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- BinopContext ------------------------------------------------------------------

qepcadParser::BinopContext::BinopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* qepcadParser::BinopContext::PLUS() {
  return getToken(qepcadParser::PLUS, 0);
}

tree::TerminalNode* qepcadParser::BinopContext::MINUS() {
  return getToken(qepcadParser::MINUS, 0);
}


size_t qepcadParser::BinopContext::getRuleIndex() const {
  return qepcadParser::RuleBinop;
}

void qepcadParser::BinopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBinop(this);
}

void qepcadParser::BinopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBinop(this);
}


antlrcpp::Any qepcadParser::BinopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<qepcadVisitor*>(visitor))
    return parserVisitor->visitBinop(this);
  else
    return visitor->visitChildren(this);
}

qepcadParser::BinopContext* qepcadParser::binop() {
  BinopContext *_localctx = _tracker.createInstance<BinopContext>(_ctx, getState());
  enterRule(_localctx, 4, qepcadParser::RuleBinop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(52);
    _la = _input->LA(1);
    if (!(_la == qepcadParser::PLUS

    || _la == qepcadParser::MINUS)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- FormulaContext ------------------------------------------------------------------

qepcadParser::FormulaContext::FormulaContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* qepcadParser::FormulaContext::BTRUE() {
  return getToken(qepcadParser::BTRUE, 0);
}

tree::TerminalNode* qepcadParser::FormulaContext::BFALSE() {
  return getToken(qepcadParser::BFALSE, 0);
}

qepcadParser::LitContext* qepcadParser::FormulaContext::lit() {
  return getRuleContext<qepcadParser::LitContext>(0);
}

tree::TerminalNode* qepcadParser::FormulaContext::LPAR() {
  return getToken(qepcadParser::LPAR, 0);
}

std::vector<qepcadParser::FormulaContext *> qepcadParser::FormulaContext::formula() {
  return getRuleContexts<qepcadParser::FormulaContext>();
}

qepcadParser::FormulaContext* qepcadParser::FormulaContext::formula(size_t i) {
  return getRuleContext<qepcadParser::FormulaContext>(i);
}

qepcadParser::BoolopContext* qepcadParser::FormulaContext::boolop() {
  return getRuleContext<qepcadParser::BoolopContext>(0);
}

tree::TerminalNode* qepcadParser::FormulaContext::RPAR() {
  return getToken(qepcadParser::RPAR, 0);
}


size_t qepcadParser::FormulaContext::getRuleIndex() const {
  return qepcadParser::RuleFormula;
}

void qepcadParser::FormulaContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFormula(this);
}

void qepcadParser::FormulaContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFormula(this);
}


antlrcpp::Any qepcadParser::FormulaContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<qepcadVisitor*>(visitor))
    return parserVisitor->visitFormula(this);
  else
    return visitor->visitChildren(this);
}


qepcadParser::FormulaContext* qepcadParser::formula() {
   return formula(0);
}

qepcadParser::FormulaContext* qepcadParser::formula(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  qepcadParser::FormulaContext *_localctx = _tracker.createInstance<FormulaContext>(_ctx, parentState);
  qepcadParser::FormulaContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 6;
  enterRecursionRule(_localctx, 6, qepcadParser::RuleFormula, precedence);

    

  auto onExit = finally([=] {
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(64);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 3, _ctx)) {
    case 1: {
      setState(55);
      match(qepcadParser::BTRUE);
      break;
    }

    case 2: {
      setState(56);
      match(qepcadParser::BFALSE);
      break;
    }

    case 3: {
      setState(57);
      lit();
      break;
    }

    case 4: {
      setState(58);
      match(qepcadParser::LPAR);
      setState(59);
      formula(0);
      setState(60);
      boolop();
      setState(61);
      formula(0);
      setState(62);
      match(qepcadParser::RPAR);
      break;
    }

    }
    _ctx->stop = _input->LT(-1);
    setState(72);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 4, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<FormulaContext>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleFormula);
        setState(66);

        if (!(precpred(_ctx, 1))) throw FailedPredicateException(this, "precpred(_ctx, 1)");
        setState(67);
        boolop();
        setState(68);
        formula(2); 
      }
      setState(74);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 4, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- LitContext ------------------------------------------------------------------

qepcadParser::LitContext::LitContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* qepcadParser::LitContext::LPAR() {
  return getToken(qepcadParser::LPAR, 0);
}

std::vector<qepcadParser::ExprContext *> qepcadParser::LitContext::expr() {
  return getRuleContexts<qepcadParser::ExprContext>();
}

qepcadParser::ExprContext* qepcadParser::LitContext::expr(size_t i) {
  return getRuleContext<qepcadParser::ExprContext>(i);
}

qepcadParser::RelopContext* qepcadParser::LitContext::relop() {
  return getRuleContext<qepcadParser::RelopContext>(0);
}

tree::TerminalNode* qepcadParser::LitContext::RPAR() {
  return getToken(qepcadParser::RPAR, 0);
}


size_t qepcadParser::LitContext::getRuleIndex() const {
  return qepcadParser::RuleLit;
}

void qepcadParser::LitContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLit(this);
}

void qepcadParser::LitContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLit(this);
}


antlrcpp::Any qepcadParser::LitContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<qepcadVisitor*>(visitor))
    return parserVisitor->visitLit(this);
  else
    return visitor->visitChildren(this);
}

qepcadParser::LitContext* qepcadParser::lit() {
  LitContext *_localctx = _tracker.createInstance<LitContext>(_ctx, getState());
  enterRule(_localctx, 8, qepcadParser::RuleLit);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    setState(85);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 5, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(75);
      match(qepcadParser::LPAR);
      setState(76);
      expr(0);
      setState(77);
      relop();
      setState(78);
      expr(0);
      setState(79);
      match(qepcadParser::RPAR);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(81);
      expr(0);
      setState(82);
      relop();
      setState(83);
      expr(0);
      break;
    }

    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BoolopContext ------------------------------------------------------------------

qepcadParser::BoolopContext::BoolopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* qepcadParser::BoolopContext::AND() {
  return getToken(qepcadParser::AND, 0);
}

tree::TerminalNode* qepcadParser::BoolopContext::OR() {
  return getToken(qepcadParser::OR, 0);
}


size_t qepcadParser::BoolopContext::getRuleIndex() const {
  return qepcadParser::RuleBoolop;
}

void qepcadParser::BoolopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBoolop(this);
}

void qepcadParser::BoolopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBoolop(this);
}


antlrcpp::Any qepcadParser::BoolopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<qepcadVisitor*>(visitor))
    return parserVisitor->visitBoolop(this);
  else
    return visitor->visitChildren(this);
}

qepcadParser::BoolopContext* qepcadParser::boolop() {
  BoolopContext *_localctx = _tracker.createInstance<BoolopContext>(_ctx, getState());
  enterRule(_localctx, 10, qepcadParser::RuleBoolop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(87);
    _la = _input->LA(1);
    if (!(_la == qepcadParser::AND

    || _la == qepcadParser::OR)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- RelopContext ------------------------------------------------------------------

qepcadParser::RelopContext::RelopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* qepcadParser::RelopContext::LT() {
  return getToken(qepcadParser::LT, 0);
}

tree::TerminalNode* qepcadParser::RelopContext::LEQ() {
  return getToken(qepcadParser::LEQ, 0);
}

tree::TerminalNode* qepcadParser::RelopContext::EQ() {
  return getToken(qepcadParser::EQ, 0);
}

tree::TerminalNode* qepcadParser::RelopContext::GT() {
  return getToken(qepcadParser::GT, 0);
}

tree::TerminalNode* qepcadParser::RelopContext::GEQ() {
  return getToken(qepcadParser::GEQ, 0);
}

tree::TerminalNode* qepcadParser::RelopContext::NEQ() {
  return getToken(qepcadParser::NEQ, 0);
}


size_t qepcadParser::RelopContext::getRuleIndex() const {
  return qepcadParser::RuleRelop;
}

void qepcadParser::RelopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterRelop(this);
}

void qepcadParser::RelopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<qepcadListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitRelop(this);
}


antlrcpp::Any qepcadParser::RelopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<qepcadVisitor*>(visitor))
    return parserVisitor->visitRelop(this);
  else
    return visitor->visitChildren(this);
}

qepcadParser::RelopContext* qepcadParser::relop() {
  RelopContext *_localctx = _tracker.createInstance<RelopContext>(_ctx, getState());
  enterRule(_localctx, 12, qepcadParser::RuleRelop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(89);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & ((1ULL << qepcadParser::LT)
      | (1ULL << qepcadParser::LEQ)
      | (1ULL << qepcadParser::EQ)
      | (1ULL << qepcadParser::NEQ)
      | (1ULL << qepcadParser::GEQ)
      | (1ULL << qepcadParser::GT))) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

bool qepcadParser::sempred(RuleContext *context, size_t ruleIndex, size_t predicateIndex) {
  switch (ruleIndex) {
    case 1: return exprSempred(dynamic_cast<ExprContext *>(context), predicateIndex);
    case 3: return formulaSempred(dynamic_cast<FormulaContext *>(context), predicateIndex);

  default:
    break;
  }
  return true;
}

bool qepcadParser::exprSempred(ExprContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 0: return precpred(_ctx, 5);
    case 1: return precpred(_ctx, 3);
    case 2: return precpred(_ctx, 1);

  default:
    break;
  }
  return true;
}

bool qepcadParser::formulaSempred(FormulaContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 3: return precpred(_ctx, 1);

  default:
    break;
  }
  return true;
}

// Static vars and initialization.
std::vector<dfa::DFA> qepcadParser::_decisionToDFA;
atn::PredictionContextCache qepcadParser::_sharedContextCache;

// We own the ATN which in turn owns the ATN states.
atn::ATN qepcadParser::_atn;
std::vector<uint16_t> qepcadParser::_serializedATN;

std::vector<std::string> qepcadParser::_ruleNames = {
  "main", "expr", "binop", "formula", "lit", "boolop", "relop"
};

std::vector<std::string> qepcadParser::_literalNames = {
  "", "'+'", "'-'", "'^'", "'['", "']'", "'/\\'", "'\\/'", "'<'", "'<='", 
  "'='", "'/='", "'>='", "'>'", "'TRUE'", "'FALSE'"
};

std::vector<std::string> qepcadParser::_symbolicNames = {
  "", "PLUS", "MINUS", "EXP", "LPAR", "RPAR", "AND", "OR", "LT", "LEQ", 
  "EQ", "NEQ", "GEQ", "GT", "BTRUE", "BFALSE", "VAR", "INT", "WS"
};

dfa::Vocabulary qepcadParser::_vocabulary(_literalNames, _symbolicNames);

std::vector<std::string> qepcadParser::_tokenNames;

qepcadParser::Initializer::Initializer() {
	for (size_t i = 0; i < _symbolicNames.size(); ++i) {
		std::string name = _vocabulary.getLiteralName(i);
		if (name.empty()) {
			name = _vocabulary.getSymbolicName(i);
		}

		if (name.empty()) {
			_tokenNames.push_back("<INVALID>");
		} else {
      _tokenNames.push_back(name);
    }
	}

  _serializedATN = {
    0x3, 0x608b, 0xa72a, 0x8133, 0xb9ed, 0x417c, 0x3be7, 0x7786, 0x5964, 
    0x3, 0x14, 0x5e, 0x4, 0x2, 0x9, 0x2, 0x4, 0x3, 0x9, 0x3, 0x4, 0x4, 0x9, 
    0x4, 0x4, 0x5, 0x9, 0x5, 0x4, 0x6, 0x9, 0x6, 0x4, 0x7, 0x9, 0x7, 0x4, 
    0x8, 0x9, 0x8, 0x3, 0x2, 0x3, 0x2, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x5, 0x3, 0x27, 0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x7, 0x3, 0x32, 0xa, 0x3, 0xc, 0x3, 0xe, 0x3, 0x35, 0xb, 0x3, 0x3, 0x4, 
    0x3, 0x4, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 
    0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x5, 0x5, 0x43, 0xa, 0x5, 0x3, 
    0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x7, 0x5, 0x49, 0xa, 0x5, 0xc, 0x5, 
    0xe, 0x5, 0x4c, 0xb, 0x5, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 
    0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x5, 0x6, 0x58, 
    0xa, 0x6, 0x3, 0x7, 0x3, 0x7, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x2, 0x4, 
    0x4, 0x8, 0x9, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, 0x2, 0x5, 0x3, 0x2, 
    0x3, 0x4, 0x3, 0x2, 0x8, 0x9, 0x3, 0x2, 0xa, 0xf, 0x2, 0x62, 0x2, 0x10, 
    0x3, 0x2, 0x2, 0x2, 0x4, 0x26, 0x3, 0x2, 0x2, 0x2, 0x6, 0x36, 0x3, 0x2, 
    0x2, 0x2, 0x8, 0x42, 0x3, 0x2, 0x2, 0x2, 0xa, 0x57, 0x3, 0x2, 0x2, 0x2, 
    0xc, 0x59, 0x3, 0x2, 0x2, 0x2, 0xe, 0x5b, 0x3, 0x2, 0x2, 0x2, 0x10, 
    0x11, 0x5, 0x8, 0x5, 0x2, 0x11, 0x3, 0x3, 0x2, 0x2, 0x2, 0x12, 0x13, 
    0x8, 0x3, 0x1, 0x2, 0x13, 0x27, 0x7, 0x12, 0x2, 0x2, 0x14, 0x27, 0x7, 
    0x13, 0x2, 0x2, 0x15, 0x16, 0x7, 0x6, 0x2, 0x2, 0x16, 0x17, 0x5, 0x4, 
    0x3, 0x2, 0x17, 0x18, 0x7, 0x5, 0x2, 0x2, 0x18, 0x19, 0x5, 0x4, 0x3, 
    0x2, 0x19, 0x1a, 0x7, 0x7, 0x2, 0x2, 0x1a, 0x27, 0x3, 0x2, 0x2, 0x2, 
    0x1b, 0x1c, 0x7, 0x6, 0x2, 0x2, 0x1c, 0x1d, 0x5, 0x4, 0x3, 0x2, 0x1d, 
    0x1e, 0x5, 0x4, 0x3, 0x2, 0x1e, 0x1f, 0x7, 0x7, 0x2, 0x2, 0x1f, 0x27, 
    0x3, 0x2, 0x2, 0x2, 0x20, 0x21, 0x7, 0x6, 0x2, 0x2, 0x21, 0x22, 0x5, 
    0x4, 0x3, 0x2, 0x22, 0x23, 0x5, 0x6, 0x4, 0x2, 0x23, 0x24, 0x5, 0x4, 
    0x3, 0x2, 0x24, 0x25, 0x7, 0x7, 0x2, 0x2, 0x25, 0x27, 0x3, 0x2, 0x2, 
    0x2, 0x26, 0x12, 0x3, 0x2, 0x2, 0x2, 0x26, 0x14, 0x3, 0x2, 0x2, 0x2, 
    0x26, 0x15, 0x3, 0x2, 0x2, 0x2, 0x26, 0x1b, 0x3, 0x2, 0x2, 0x2, 0x26, 
    0x20, 0x3, 0x2, 0x2, 0x2, 0x27, 0x33, 0x3, 0x2, 0x2, 0x2, 0x28, 0x29, 
    0xc, 0x7, 0x2, 0x2, 0x29, 0x2a, 0x7, 0x5, 0x2, 0x2, 0x2a, 0x32, 0x5, 
    0x4, 0x3, 0x8, 0x2b, 0x2c, 0xc, 0x5, 0x2, 0x2, 0x2c, 0x32, 0x5, 0x4, 
    0x3, 0x6, 0x2d, 0x2e, 0xc, 0x3, 0x2, 0x2, 0x2e, 0x2f, 0x5, 0x6, 0x4, 
    0x2, 0x2f, 0x30, 0x5, 0x4, 0x3, 0x4, 0x30, 0x32, 0x3, 0x2, 0x2, 0x2, 
    0x31, 0x28, 0x3, 0x2, 0x2, 0x2, 0x31, 0x2b, 0x3, 0x2, 0x2, 0x2, 0x31, 
    0x2d, 0x3, 0x2, 0x2, 0x2, 0x32, 0x35, 0x3, 0x2, 0x2, 0x2, 0x33, 0x31, 
    0x3, 0x2, 0x2, 0x2, 0x33, 0x34, 0x3, 0x2, 0x2, 0x2, 0x34, 0x5, 0x3, 
    0x2, 0x2, 0x2, 0x35, 0x33, 0x3, 0x2, 0x2, 0x2, 0x36, 0x37, 0x9, 0x2, 
    0x2, 0x2, 0x37, 0x7, 0x3, 0x2, 0x2, 0x2, 0x38, 0x39, 0x8, 0x5, 0x1, 
    0x2, 0x39, 0x43, 0x7, 0x10, 0x2, 0x2, 0x3a, 0x43, 0x7, 0x11, 0x2, 0x2, 
    0x3b, 0x43, 0x5, 0xa, 0x6, 0x2, 0x3c, 0x3d, 0x7, 0x6, 0x2, 0x2, 0x3d, 
    0x3e, 0x5, 0x8, 0x5, 0x2, 0x3e, 0x3f, 0x5, 0xc, 0x7, 0x2, 0x3f, 0x40, 
    0x5, 0x8, 0x5, 0x2, 0x40, 0x41, 0x7, 0x7, 0x2, 0x2, 0x41, 0x43, 0x3, 
    0x2, 0x2, 0x2, 0x42, 0x38, 0x3, 0x2, 0x2, 0x2, 0x42, 0x3a, 0x3, 0x2, 
    0x2, 0x2, 0x42, 0x3b, 0x3, 0x2, 0x2, 0x2, 0x42, 0x3c, 0x3, 0x2, 0x2, 
    0x2, 0x43, 0x4a, 0x3, 0x2, 0x2, 0x2, 0x44, 0x45, 0xc, 0x3, 0x2, 0x2, 
    0x45, 0x46, 0x5, 0xc, 0x7, 0x2, 0x46, 0x47, 0x5, 0x8, 0x5, 0x4, 0x47, 
    0x49, 0x3, 0x2, 0x2, 0x2, 0x48, 0x44, 0x3, 0x2, 0x2, 0x2, 0x49, 0x4c, 
    0x3, 0x2, 0x2, 0x2, 0x4a, 0x48, 0x3, 0x2, 0x2, 0x2, 0x4a, 0x4b, 0x3, 
    0x2, 0x2, 0x2, 0x4b, 0x9, 0x3, 0x2, 0x2, 0x2, 0x4c, 0x4a, 0x3, 0x2, 
    0x2, 0x2, 0x4d, 0x4e, 0x7, 0x6, 0x2, 0x2, 0x4e, 0x4f, 0x5, 0x4, 0x3, 
    0x2, 0x4f, 0x50, 0x5, 0xe, 0x8, 0x2, 0x50, 0x51, 0x5, 0x4, 0x3, 0x2, 
    0x51, 0x52, 0x7, 0x7, 0x2, 0x2, 0x52, 0x58, 0x3, 0x2, 0x2, 0x2, 0x53, 
    0x54, 0x5, 0x4, 0x3, 0x2, 0x54, 0x55, 0x5, 0xe, 0x8, 0x2, 0x55, 0x56, 
    0x5, 0x4, 0x3, 0x2, 0x56, 0x58, 0x3, 0x2, 0x2, 0x2, 0x57, 0x4d, 0x3, 
    0x2, 0x2, 0x2, 0x57, 0x53, 0x3, 0x2, 0x2, 0x2, 0x58, 0xb, 0x3, 0x2, 
    0x2, 0x2, 0x59, 0x5a, 0x9, 0x3, 0x2, 0x2, 0x5a, 0xd, 0x3, 0x2, 0x2, 
    0x2, 0x5b, 0x5c, 0x9, 0x4, 0x2, 0x2, 0x5c, 0xf, 0x3, 0x2, 0x2, 0x2, 
    0x8, 0x26, 0x31, 0x33, 0x42, 0x4a, 0x57, 
  };

  atn::ATNDeserializer deserializer;
  _atn = deserializer.deserialize(_serializedATN);

  size_t count = _atn.getNumberOfDecisions();
  _decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    _decisionToDFA.emplace_back(_atn.getDecisionState(i), i);
  }
}

qepcadParser::Initializer qepcadParser::_init;
