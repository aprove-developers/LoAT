
// Generated from redlog.g4 by ANTLR 4.7.2


#include "redlogListener.h"
#include "redlogVisitor.h"

#include "redlogParser.h"


using namespace antlrcpp;
using namespace antlr4;

redlogParser::redlogParser(TokenStream *input) : Parser(input) {
  _interpreter = new atn::ParserATNSimulator(this, _atn, _decisionToDFA, _sharedContextCache);
}

redlogParser::~redlogParser() {
  delete _interpreter;
}

std::string redlogParser::getGrammarFileName() const {
  return "redlog.g4";
}

const std::vector<std::string>& redlogParser::getRuleNames() const {
  return _ruleNames;
}

dfa::Vocabulary& redlogParser::getVocabulary() const {
  return _vocabulary;
}


//----------------- MainContext ------------------------------------------------------------------

redlogParser::MainContext::MainContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

redlogParser::FormulaContext* redlogParser::MainContext::formula() {
  return getRuleContext<redlogParser::FormulaContext>(0);
}


size_t redlogParser::MainContext::getRuleIndex() const {
  return redlogParser::RuleMain;
}

void redlogParser::MainContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterMain(this);
}

void redlogParser::MainContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitMain(this);
}


antlrcpp::Any redlogParser::MainContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitMain(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::MainContext* redlogParser::main() {
  MainContext *_localctx = _tracker.createInstance<MainContext>(_ctx, getState());
  enterRule(_localctx, 0, redlogParser::RuleMain);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(16);
    formula();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprContext ------------------------------------------------------------------

redlogParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* redlogParser::ExprContext::VAR() {
  return getToken(redlogParser::VAR, 0);
}

tree::TerminalNode* redlogParser::ExprContext::INT() {
  return getToken(redlogParser::INT, 0);
}

tree::TerminalNode* redlogParser::ExprContext::LPAR() {
  return getToken(redlogParser::LPAR, 0);
}

tree::TerminalNode* redlogParser::ExprContext::MINUS() {
  return getToken(redlogParser::MINUS, 0);
}

std::vector<redlogParser::ExprContext *> redlogParser::ExprContext::expr() {
  return getRuleContexts<redlogParser::ExprContext>();
}

redlogParser::ExprContext* redlogParser::ExprContext::expr(size_t i) {
  return getRuleContext<redlogParser::ExprContext>(i);
}

tree::TerminalNode* redlogParser::ExprContext::RPAR() {
  return getToken(redlogParser::RPAR, 0);
}

redlogParser::BinopContext* redlogParser::ExprContext::binop() {
  return getRuleContext<redlogParser::BinopContext>(0);
}

redlogParser::CaopContext* redlogParser::ExprContext::caop() {
  return getRuleContext<redlogParser::CaopContext>(0);
}


size_t redlogParser::ExprContext::getRuleIndex() const {
  return redlogParser::RuleExpr;
}

void redlogParser::ExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExpr(this);
}

void redlogParser::ExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExpr(this);
}


antlrcpp::Any redlogParser::ExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitExpr(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::ExprContext* redlogParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 2, redlogParser::RuleExpr);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    setState(41);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 1, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(18);
      match(redlogParser::VAR);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(19);
      match(redlogParser::INT);
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(20);
      match(redlogParser::LPAR);
      setState(21);
      match(redlogParser::MINUS);
      setState(22);
      expr();
      setState(23);
      match(redlogParser::RPAR);
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(25);
      match(redlogParser::LPAR);
      setState(26);
      binop();
      setState(27);
      expr();
      setState(28);
      expr();
      setState(29);
      match(redlogParser::RPAR);
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(31);
      match(redlogParser::LPAR);
      setState(32);
      caop();
      setState(36);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & ((1ULL << redlogParser::LPAR)
        | (1ULL << redlogParser::VAR)
        | (1ULL << redlogParser::INT))) != 0)) {
        setState(33);
        expr();
        setState(38);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
      setState(39);
      match(redlogParser::RPAR);
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

//----------------- CaopContext ------------------------------------------------------------------

redlogParser::CaopContext::CaopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* redlogParser::CaopContext::TIMES() {
  return getToken(redlogParser::TIMES, 0);
}

tree::TerminalNode* redlogParser::CaopContext::PLUS() {
  return getToken(redlogParser::PLUS, 0);
}


size_t redlogParser::CaopContext::getRuleIndex() const {
  return redlogParser::RuleCaop;
}

void redlogParser::CaopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterCaop(this);
}

void redlogParser::CaopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitCaop(this);
}


antlrcpp::Any redlogParser::CaopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitCaop(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::CaopContext* redlogParser::caop() {
  CaopContext *_localctx = _tracker.createInstance<CaopContext>(_ctx, getState());
  enterRule(_localctx, 4, redlogParser::RuleCaop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(43);
    _la = _input->LA(1);
    if (!(_la == redlogParser::PLUS

    || _la == redlogParser::TIMES)) {
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

//----------------- BinopContext ------------------------------------------------------------------

redlogParser::BinopContext::BinopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* redlogParser::BinopContext::EXP() {
  return getToken(redlogParser::EXP, 0);
}

tree::TerminalNode* redlogParser::BinopContext::MINUS() {
  return getToken(redlogParser::MINUS, 0);
}


size_t redlogParser::BinopContext::getRuleIndex() const {
  return redlogParser::RuleBinop;
}

void redlogParser::BinopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBinop(this);
}

void redlogParser::BinopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBinop(this);
}


antlrcpp::Any redlogParser::BinopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitBinop(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::BinopContext* redlogParser::binop() {
  BinopContext *_localctx = _tracker.createInstance<BinopContext>(_ctx, getState());
  enterRule(_localctx, 6, redlogParser::RuleBinop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(45);
    _la = _input->LA(1);
    if (!(_la == redlogParser::MINUS

    || _la == redlogParser::EXP)) {
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

redlogParser::FormulaContext::FormulaContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* redlogParser::FormulaContext::TRUE() {
  return getToken(redlogParser::TRUE, 0);
}

tree::TerminalNode* redlogParser::FormulaContext::FALSE() {
  return getToken(redlogParser::FALSE, 0);
}

redlogParser::LitContext* redlogParser::FormulaContext::lit() {
  return getRuleContext<redlogParser::LitContext>(0);
}

tree::TerminalNode* redlogParser::FormulaContext::LPAR() {
  return getToken(redlogParser::LPAR, 0);
}

redlogParser::BoolopContext* redlogParser::FormulaContext::boolop() {
  return getRuleContext<redlogParser::BoolopContext>(0);
}

tree::TerminalNode* redlogParser::FormulaContext::RPAR() {
  return getToken(redlogParser::RPAR, 0);
}

std::vector<redlogParser::FormulaContext *> redlogParser::FormulaContext::formula() {
  return getRuleContexts<redlogParser::FormulaContext>();
}

redlogParser::FormulaContext* redlogParser::FormulaContext::formula(size_t i) {
  return getRuleContext<redlogParser::FormulaContext>(i);
}


size_t redlogParser::FormulaContext::getRuleIndex() const {
  return redlogParser::RuleFormula;
}

void redlogParser::FormulaContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFormula(this);
}

void redlogParser::FormulaContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFormula(this);
}


antlrcpp::Any redlogParser::FormulaContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitFormula(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::FormulaContext* redlogParser::formula() {
  FormulaContext *_localctx = _tracker.createInstance<FormulaContext>(_ctx, getState());
  enterRule(_localctx, 8, redlogParser::RuleFormula);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    setState(60);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 3, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(47);
      match(redlogParser::TRUE);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(48);
      match(redlogParser::FALSE);
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(49);
      lit();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(50);
      match(redlogParser::LPAR);
      setState(51);
      boolop();
      setState(55);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & ((1ULL << redlogParser::LPAR)
        | (1ULL << redlogParser::TRUE)
        | (1ULL << redlogParser::FALSE))) != 0)) {
        setState(52);
        formula();
        setState(57);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
      setState(58);
      match(redlogParser::RPAR);
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

//----------------- LitContext ------------------------------------------------------------------

redlogParser::LitContext::LitContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* redlogParser::LitContext::LPAR() {
  return getToken(redlogParser::LPAR, 0);
}

redlogParser::RelopContext* redlogParser::LitContext::relop() {
  return getRuleContext<redlogParser::RelopContext>(0);
}

std::vector<redlogParser::ExprContext *> redlogParser::LitContext::expr() {
  return getRuleContexts<redlogParser::ExprContext>();
}

redlogParser::ExprContext* redlogParser::LitContext::expr(size_t i) {
  return getRuleContext<redlogParser::ExprContext>(i);
}

tree::TerminalNode* redlogParser::LitContext::RPAR() {
  return getToken(redlogParser::RPAR, 0);
}


size_t redlogParser::LitContext::getRuleIndex() const {
  return redlogParser::RuleLit;
}

void redlogParser::LitContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLit(this);
}

void redlogParser::LitContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLit(this);
}


antlrcpp::Any redlogParser::LitContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitLit(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::LitContext* redlogParser::lit() {
  LitContext *_localctx = _tracker.createInstance<LitContext>(_ctx, getState());
  enterRule(_localctx, 10, redlogParser::RuleLit);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(62);
    match(redlogParser::LPAR);
    setState(63);
    relop();
    setState(64);
    expr();
    setState(65);
    expr();
    setState(66);
    match(redlogParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BoolopContext ------------------------------------------------------------------

redlogParser::BoolopContext::BoolopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* redlogParser::BoolopContext::AND() {
  return getToken(redlogParser::AND, 0);
}

tree::TerminalNode* redlogParser::BoolopContext::OR() {
  return getToken(redlogParser::OR, 0);
}


size_t redlogParser::BoolopContext::getRuleIndex() const {
  return redlogParser::RuleBoolop;
}

void redlogParser::BoolopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBoolop(this);
}

void redlogParser::BoolopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBoolop(this);
}


antlrcpp::Any redlogParser::BoolopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitBoolop(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::BoolopContext* redlogParser::boolop() {
  BoolopContext *_localctx = _tracker.createInstance<BoolopContext>(_ctx, getState());
  enterRule(_localctx, 12, redlogParser::RuleBoolop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(68);
    _la = _input->LA(1);
    if (!(_la == redlogParser::AND

    || _la == redlogParser::OR)) {
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

redlogParser::RelopContext::RelopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* redlogParser::RelopContext::LT() {
  return getToken(redlogParser::LT, 0);
}

tree::TerminalNode* redlogParser::RelopContext::LEQ() {
  return getToken(redlogParser::LEQ, 0);
}

tree::TerminalNode* redlogParser::RelopContext::EQ() {
  return getToken(redlogParser::EQ, 0);
}

tree::TerminalNode* redlogParser::RelopContext::GT() {
  return getToken(redlogParser::GT, 0);
}

tree::TerminalNode* redlogParser::RelopContext::GEQ() {
  return getToken(redlogParser::GEQ, 0);
}

tree::TerminalNode* redlogParser::RelopContext::NEQ() {
  return getToken(redlogParser::NEQ, 0);
}


size_t redlogParser::RelopContext::getRuleIndex() const {
  return redlogParser::RuleRelop;
}

void redlogParser::RelopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterRelop(this);
}

void redlogParser::RelopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<redlogListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitRelop(this);
}


antlrcpp::Any redlogParser::RelopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<redlogVisitor*>(visitor))
    return parserVisitor->visitRelop(this);
  else
    return visitor->visitChildren(this);
}

redlogParser::RelopContext* redlogParser::relop() {
  RelopContext *_localctx = _tracker.createInstance<RelopContext>(_ctx, getState());
  enterRule(_localctx, 14, redlogParser::RuleRelop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(70);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & ((1ULL << redlogParser::LT)
      | (1ULL << redlogParser::LEQ)
      | (1ULL << redlogParser::EQ)
      | (1ULL << redlogParser::NEQ)
      | (1ULL << redlogParser::GEQ)
      | (1ULL << redlogParser::GT))) != 0))) {
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

// Static vars and initialization.
std::vector<dfa::DFA> redlogParser::_decisionToDFA;
atn::PredictionContextCache redlogParser::_sharedContextCache;

// We own the ATN which in turn owns the ATN states.
atn::ATN redlogParser::_atn;
std::vector<uint16_t> redlogParser::_serializedATN;

std::vector<std::string> redlogParser::_ruleNames = {
  "main", "expr", "caop", "binop", "formula", "lit", "boolop", "relop"
};

std::vector<std::string> redlogParser::_literalNames = {
  "", "'plus'", "'minus'", "'times'", "'expt'", "'('", "')'", "'and'", "'or'", 
  "'lessp'", "'leq'", "'equal'", "'neq'", "'geq'", "'greaterp'", "'true'", 
  "'false'"
};

std::vector<std::string> redlogParser::_symbolicNames = {
  "", "PLUS", "MINUS", "TIMES", "EXP", "LPAR", "RPAR", "AND", "OR", "LT", 
  "LEQ", "EQ", "NEQ", "GEQ", "GT", "TRUE", "FALSE", "VAR", "INT", "WS"
};

dfa::Vocabulary redlogParser::_vocabulary(_literalNames, _symbolicNames);

std::vector<std::string> redlogParser::_tokenNames;

redlogParser::Initializer::Initializer() {
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
    0x3, 0x15, 0x4b, 0x4, 0x2, 0x9, 0x2, 0x4, 0x3, 0x9, 0x3, 0x4, 0x4, 0x9, 
    0x4, 0x4, 0x5, 0x9, 0x5, 0x4, 0x6, 0x9, 0x6, 0x4, 0x7, 0x9, 0x7, 0x4, 
    0x8, 0x9, 0x8, 0x4, 0x9, 0x9, 0x9, 0x3, 0x2, 0x3, 0x2, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x7, 0x3, 0x25, 0xa, 0x3, 0xc, 0x3, 0xe, 0x3, 0x28, 0xb, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x5, 0x3, 0x2c, 0xa, 0x3, 0x3, 0x4, 0x3, 0x4, 0x3, 0x5, 
    0x3, 0x5, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 
    0x7, 0x6, 0x38, 0xa, 0x6, 0xc, 0x6, 0xe, 0x6, 0x3b, 0xb, 0x6, 0x3, 0x6, 
    0x3, 0x6, 0x5, 0x6, 0x3f, 0xa, 0x6, 0x3, 0x7, 0x3, 0x7, 0x3, 0x7, 0x3, 
    0x7, 0x3, 0x7, 0x3, 0x7, 0x3, 0x8, 0x3, 0x8, 0x3, 0x9, 0x3, 0x9, 0x3, 
    0x9, 0x2, 0x2, 0xa, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, 0x10, 0x2, 0x6, 
    0x4, 0x2, 0x3, 0x3, 0x5, 0x5, 0x4, 0x2, 0x4, 0x4, 0x6, 0x6, 0x3, 0x2, 
    0x9, 0xa, 0x3, 0x2, 0xb, 0x10, 0x2, 0x4b, 0x2, 0x12, 0x3, 0x2, 0x2, 
    0x2, 0x4, 0x2b, 0x3, 0x2, 0x2, 0x2, 0x6, 0x2d, 0x3, 0x2, 0x2, 0x2, 0x8, 
    0x2f, 0x3, 0x2, 0x2, 0x2, 0xa, 0x3e, 0x3, 0x2, 0x2, 0x2, 0xc, 0x40, 
    0x3, 0x2, 0x2, 0x2, 0xe, 0x46, 0x3, 0x2, 0x2, 0x2, 0x10, 0x48, 0x3, 
    0x2, 0x2, 0x2, 0x12, 0x13, 0x5, 0xa, 0x6, 0x2, 0x13, 0x3, 0x3, 0x2, 
    0x2, 0x2, 0x14, 0x2c, 0x7, 0x13, 0x2, 0x2, 0x15, 0x2c, 0x7, 0x14, 0x2, 
    0x2, 0x16, 0x17, 0x7, 0x7, 0x2, 0x2, 0x17, 0x18, 0x7, 0x4, 0x2, 0x2, 
    0x18, 0x19, 0x5, 0x4, 0x3, 0x2, 0x19, 0x1a, 0x7, 0x8, 0x2, 0x2, 0x1a, 
    0x2c, 0x3, 0x2, 0x2, 0x2, 0x1b, 0x1c, 0x7, 0x7, 0x2, 0x2, 0x1c, 0x1d, 
    0x5, 0x8, 0x5, 0x2, 0x1d, 0x1e, 0x5, 0x4, 0x3, 0x2, 0x1e, 0x1f, 0x5, 
    0x4, 0x3, 0x2, 0x1f, 0x20, 0x7, 0x8, 0x2, 0x2, 0x20, 0x2c, 0x3, 0x2, 
    0x2, 0x2, 0x21, 0x22, 0x7, 0x7, 0x2, 0x2, 0x22, 0x26, 0x5, 0x6, 0x4, 
    0x2, 0x23, 0x25, 0x5, 0x4, 0x3, 0x2, 0x24, 0x23, 0x3, 0x2, 0x2, 0x2, 
    0x25, 0x28, 0x3, 0x2, 0x2, 0x2, 0x26, 0x24, 0x3, 0x2, 0x2, 0x2, 0x26, 
    0x27, 0x3, 0x2, 0x2, 0x2, 0x27, 0x29, 0x3, 0x2, 0x2, 0x2, 0x28, 0x26, 
    0x3, 0x2, 0x2, 0x2, 0x29, 0x2a, 0x7, 0x8, 0x2, 0x2, 0x2a, 0x2c, 0x3, 
    0x2, 0x2, 0x2, 0x2b, 0x14, 0x3, 0x2, 0x2, 0x2, 0x2b, 0x15, 0x3, 0x2, 
    0x2, 0x2, 0x2b, 0x16, 0x3, 0x2, 0x2, 0x2, 0x2b, 0x1b, 0x3, 0x2, 0x2, 
    0x2, 0x2b, 0x21, 0x3, 0x2, 0x2, 0x2, 0x2c, 0x5, 0x3, 0x2, 0x2, 0x2, 
    0x2d, 0x2e, 0x9, 0x2, 0x2, 0x2, 0x2e, 0x7, 0x3, 0x2, 0x2, 0x2, 0x2f, 
    0x30, 0x9, 0x3, 0x2, 0x2, 0x30, 0x9, 0x3, 0x2, 0x2, 0x2, 0x31, 0x3f, 
    0x7, 0x11, 0x2, 0x2, 0x32, 0x3f, 0x7, 0x12, 0x2, 0x2, 0x33, 0x3f, 0x5, 
    0xc, 0x7, 0x2, 0x34, 0x35, 0x7, 0x7, 0x2, 0x2, 0x35, 0x39, 0x5, 0xe, 
    0x8, 0x2, 0x36, 0x38, 0x5, 0xa, 0x6, 0x2, 0x37, 0x36, 0x3, 0x2, 0x2, 
    0x2, 0x38, 0x3b, 0x3, 0x2, 0x2, 0x2, 0x39, 0x37, 0x3, 0x2, 0x2, 0x2, 
    0x39, 0x3a, 0x3, 0x2, 0x2, 0x2, 0x3a, 0x3c, 0x3, 0x2, 0x2, 0x2, 0x3b, 
    0x39, 0x3, 0x2, 0x2, 0x2, 0x3c, 0x3d, 0x7, 0x8, 0x2, 0x2, 0x3d, 0x3f, 
    0x3, 0x2, 0x2, 0x2, 0x3e, 0x31, 0x3, 0x2, 0x2, 0x2, 0x3e, 0x32, 0x3, 
    0x2, 0x2, 0x2, 0x3e, 0x33, 0x3, 0x2, 0x2, 0x2, 0x3e, 0x34, 0x3, 0x2, 
    0x2, 0x2, 0x3f, 0xb, 0x3, 0x2, 0x2, 0x2, 0x40, 0x41, 0x7, 0x7, 0x2, 
    0x2, 0x41, 0x42, 0x5, 0x10, 0x9, 0x2, 0x42, 0x43, 0x5, 0x4, 0x3, 0x2, 
    0x43, 0x44, 0x5, 0x4, 0x3, 0x2, 0x44, 0x45, 0x7, 0x8, 0x2, 0x2, 0x45, 
    0xd, 0x3, 0x2, 0x2, 0x2, 0x46, 0x47, 0x9, 0x4, 0x2, 0x2, 0x47, 0xf, 
    0x3, 0x2, 0x2, 0x2, 0x48, 0x49, 0x9, 0x5, 0x2, 0x2, 0x49, 0x11, 0x3, 
    0x2, 0x2, 0x2, 0x6, 0x26, 0x2b, 0x39, 0x3e, 
  };

  atn::ATNDeserializer deserializer;
  _atn = deserializer.deserialize(_serializedATN);

  size_t count = _atn.getNumberOfDecisions();
  _decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    _decisionToDFA.emplace_back(_atn.getDecisionState(i), i);
  }
}

redlogParser::Initializer redlogParser::_init;
