
// Generated from Koat.g4 by ANTLR 4.7.2


#include "KoatListener.h"
#include "KoatVisitor.h"

#include "KoatParser.h"


using namespace antlrcpp;
using namespace antlr4;

KoatParser::KoatParser(TokenStream *input) : Parser(input) {
  _interpreter = new atn::ParserATNSimulator(this, _atn, _decisionToDFA, _sharedContextCache);
}

KoatParser::~KoatParser() {
  delete _interpreter;
}

std::string KoatParser::getGrammarFileName() const {
  return "Koat.g4";
}

const std::vector<std::string>& KoatParser::getRuleNames() const {
  return _ruleNames;
}

dfa::Vocabulary& KoatParser::getVocabulary() const {
  return _vocabulary;
}


//----------------- MainContext ------------------------------------------------------------------

KoatParser::MainContext::MainContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::StartContext* KoatParser::MainContext::start() {
  return getRuleContext<KoatParser::StartContext>(0);
}

KoatParser::VardeclContext* KoatParser::MainContext::vardecl() {
  return getRuleContext<KoatParser::VardeclContext>(0);
}

KoatParser::TranssContext* KoatParser::MainContext::transs() {
  return getRuleContext<KoatParser::TranssContext>(0);
}

KoatParser::GoalContext* KoatParser::MainContext::goal() {
  return getRuleContext<KoatParser::GoalContext>(0);
}


size_t KoatParser::MainContext::getRuleIndex() const {
  return KoatParser::RuleMain;
}

void KoatParser::MainContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterMain(this);
}

void KoatParser::MainContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitMain(this);
}


antlrcpp::Any KoatParser::MainContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitMain(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::MainContext* KoatParser::main() {
  MainContext *_localctx = _tracker.createInstance<MainContext>(_ctx, getState());
  enterRule(_localctx, 0, KoatParser::RuleMain);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(43);
    _errHandler->sync(this);

    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      setState(42);
      goal();
      break;
    }

    }
    setState(45);
    start();
    setState(46);
    vardecl();
    setState(47);
    transs();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- FsContext ------------------------------------------------------------------

KoatParser::FsContext::FsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::FsContext::ID() {
  return getToken(KoatParser::ID, 0);
}


size_t KoatParser::FsContext::getRuleIndex() const {
  return KoatParser::RuleFs;
}

void KoatParser::FsContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFs(this);
}

void KoatParser::FsContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFs(this);
}


antlrcpp::Any KoatParser::FsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitFs(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::FsContext* KoatParser::fs() {
  FsContext *_localctx = _tracker.createInstance<FsContext>(_ctx, getState());
  enterRule(_localctx, 2, KoatParser::RuleFs);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(49);
    match(KoatParser::ID);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- VarContext ------------------------------------------------------------------

KoatParser::VarContext::VarContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::VarContext::ID() {
  return getToken(KoatParser::ID, 0);
}


size_t KoatParser::VarContext::getRuleIndex() const {
  return KoatParser::RuleVar;
}

void KoatParser::VarContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterVar(this);
}

void KoatParser::VarContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitVar(this);
}


antlrcpp::Any KoatParser::VarContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitVar(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::VarContext* KoatParser::var() {
  VarContext *_localctx = _tracker.createInstance<VarContext>(_ctx, getState());
  enterRule(_localctx, 4, KoatParser::RuleVar);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(51);
    match(KoatParser::ID);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- GoalContext ------------------------------------------------------------------

KoatParser::GoalContext::GoalContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::GoalContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

tree::TerminalNode* KoatParser::GoalContext::GOAL() {
  return getToken(KoatParser::GOAL, 0);
}

tree::TerminalNode* KoatParser::GoalContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

tree::TerminalNode* KoatParser::GoalContext::CPX() {
  return getToken(KoatParser::CPX, 0);
}

tree::TerminalNode* KoatParser::GoalContext::TERM() {
  return getToken(KoatParser::TERM, 0);
}


size_t KoatParser::GoalContext::getRuleIndex() const {
  return KoatParser::RuleGoal;
}

void KoatParser::GoalContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterGoal(this);
}

void KoatParser::GoalContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitGoal(this);
}


antlrcpp::Any KoatParser::GoalContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitGoal(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::GoalContext* KoatParser::goal() {
  GoalContext *_localctx = _tracker.createInstance<GoalContext>(_ctx, getState());
  enterRule(_localctx, 6, KoatParser::RuleGoal);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(53);
    match(KoatParser::LPAR);
    setState(54);
    match(KoatParser::GOAL);
    setState(55);
    _la = _input->LA(1);
    if (!(_la == KoatParser::CPX

    || _la == KoatParser::TERM)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(56);
    match(KoatParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StartContext ------------------------------------------------------------------

KoatParser::StartContext::StartContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> KoatParser::StartContext::LPAR() {
  return getTokens(KoatParser::LPAR);
}

tree::TerminalNode* KoatParser::StartContext::LPAR(size_t i) {
  return getToken(KoatParser::LPAR, i);
}

tree::TerminalNode* KoatParser::StartContext::START() {
  return getToken(KoatParser::START, 0);
}

tree::TerminalNode* KoatParser::StartContext::FS() {
  return getToken(KoatParser::FS, 0);
}

KoatParser::FsContext* KoatParser::StartContext::fs() {
  return getRuleContext<KoatParser::FsContext>(0);
}

std::vector<tree::TerminalNode *> KoatParser::StartContext::RPAR() {
  return getTokens(KoatParser::RPAR);
}

tree::TerminalNode* KoatParser::StartContext::RPAR(size_t i) {
  return getToken(KoatParser::RPAR, i);
}


size_t KoatParser::StartContext::getRuleIndex() const {
  return KoatParser::RuleStart;
}

void KoatParser::StartContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterStart(this);
}

void KoatParser::StartContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitStart(this);
}


antlrcpp::Any KoatParser::StartContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitStart(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::StartContext* KoatParser::start() {
  StartContext *_localctx = _tracker.createInstance<StartContext>(_ctx, getState());
  enterRule(_localctx, 8, KoatParser::RuleStart);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(58);
    match(KoatParser::LPAR);
    setState(59);
    match(KoatParser::START);
    setState(60);
    match(KoatParser::LPAR);
    setState(61);
    match(KoatParser::FS);
    setState(62);
    fs();
    setState(63);
    match(KoatParser::RPAR);
    setState(64);
    match(KoatParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- VardeclContext ------------------------------------------------------------------

KoatParser::VardeclContext::VardeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::VardeclContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

tree::TerminalNode* KoatParser::VardeclContext::VAR() {
  return getToken(KoatParser::VAR, 0);
}

tree::TerminalNode* KoatParser::VardeclContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

std::vector<tree::TerminalNode *> KoatParser::VardeclContext::ID() {
  return getTokens(KoatParser::ID);
}

tree::TerminalNode* KoatParser::VardeclContext::ID(size_t i) {
  return getToken(KoatParser::ID, i);
}


size_t KoatParser::VardeclContext::getRuleIndex() const {
  return KoatParser::RuleVardecl;
}

void KoatParser::VardeclContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterVardecl(this);
}

void KoatParser::VardeclContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitVardecl(this);
}


antlrcpp::Any KoatParser::VardeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitVardecl(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::VardeclContext* KoatParser::vardecl() {
  VardeclContext *_localctx = _tracker.createInstance<VardeclContext>(_ctx, getState());
  enterRule(_localctx, 10, KoatParser::RuleVardecl);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(66);
    match(KoatParser::LPAR);
    setState(67);
    match(KoatParser::VAR);
    setState(69); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(68);
      match(KoatParser::ID);
      setState(71); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (_la == KoatParser::ID);
    setState(73);
    match(KoatParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TranssContext ------------------------------------------------------------------

KoatParser::TranssContext::TranssContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::TranssContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

tree::TerminalNode* KoatParser::TranssContext::RULES() {
  return getToken(KoatParser::RULES, 0);
}

tree::TerminalNode* KoatParser::TranssContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

std::vector<KoatParser::TransContext *> KoatParser::TranssContext::trans() {
  return getRuleContexts<KoatParser::TransContext>();
}

KoatParser::TransContext* KoatParser::TranssContext::trans(size_t i) {
  return getRuleContext<KoatParser::TransContext>(i);
}


size_t KoatParser::TranssContext::getRuleIndex() const {
  return KoatParser::RuleTranss;
}

void KoatParser::TranssContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTranss(this);
}

void KoatParser::TranssContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTranss(this);
}


antlrcpp::Any KoatParser::TranssContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitTranss(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::TranssContext* KoatParser::transs() {
  TranssContext *_localctx = _tracker.createInstance<TranssContext>(_ctx, getState());
  enterRule(_localctx, 12, KoatParser::RuleTranss);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(75);
    match(KoatParser::LPAR);
    setState(76);
    match(KoatParser::RULES);
    setState(80);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == KoatParser::ID) {
      setState(77);
      trans();
      setState(82);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(83);
    match(KoatParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TransContext ------------------------------------------------------------------

KoatParser::TransContext::TransContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::LhsContext* KoatParser::TransContext::lhs() {
  return getRuleContext<KoatParser::LhsContext>(0);
}

KoatParser::ToContext* KoatParser::TransContext::to() {
  return getRuleContext<KoatParser::ToContext>(0);
}

KoatParser::ComContext* KoatParser::TransContext::com() {
  return getRuleContext<KoatParser::ComContext>(0);
}

KoatParser::CondContext* KoatParser::TransContext::cond() {
  return getRuleContext<KoatParser::CondContext>(0);
}


size_t KoatParser::TransContext::getRuleIndex() const {
  return KoatParser::RuleTrans;
}

void KoatParser::TransContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTrans(this);
}

void KoatParser::TransContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTrans(this);
}


antlrcpp::Any KoatParser::TransContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitTrans(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::TransContext* KoatParser::trans() {
  TransContext *_localctx = _tracker.createInstance<TransContext>(_ctx, getState());
  enterRule(_localctx, 14, KoatParser::RuleTrans);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(85);
    lhs();
    setState(86);
    to();
    setState(87);
    com();
    setState(89);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == KoatParser::LBRACK

    || _la == KoatParser::CONDSEP) {
      setState(88);
      cond();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LhsContext ------------------------------------------------------------------

KoatParser::LhsContext::LhsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::FsContext* KoatParser::LhsContext::fs() {
  return getRuleContext<KoatParser::FsContext>(0);
}

tree::TerminalNode* KoatParser::LhsContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

tree::TerminalNode* KoatParser::LhsContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

std::vector<KoatParser::VarContext *> KoatParser::LhsContext::var() {
  return getRuleContexts<KoatParser::VarContext>();
}

KoatParser::VarContext* KoatParser::LhsContext::var(size_t i) {
  return getRuleContext<KoatParser::VarContext>(i);
}

std::vector<tree::TerminalNode *> KoatParser::LhsContext::COMMA() {
  return getTokens(KoatParser::COMMA);
}

tree::TerminalNode* KoatParser::LhsContext::COMMA(size_t i) {
  return getToken(KoatParser::COMMA, i);
}


size_t KoatParser::LhsContext::getRuleIndex() const {
  return KoatParser::RuleLhs;
}

void KoatParser::LhsContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLhs(this);
}

void KoatParser::LhsContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLhs(this);
}


antlrcpp::Any KoatParser::LhsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitLhs(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::LhsContext* KoatParser::lhs() {
  LhsContext *_localctx = _tracker.createInstance<LhsContext>(_ctx, getState());
  enterRule(_localctx, 16, KoatParser::RuleLhs);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(91);
    fs();
    setState(92);
    match(KoatParser::LPAR);
    setState(101);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == KoatParser::ID) {
      setState(93);
      var();
      setState(98);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == KoatParser::COMMA) {
        setState(94);
        match(KoatParser::COMMA);
        setState(95);
        var();
        setState(100);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(103);
    match(KoatParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ComContext ------------------------------------------------------------------

KoatParser::ComContext::ComContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::ComContext::COM() {
  return getToken(KoatParser::COM, 0);
}

tree::TerminalNode* KoatParser::ComContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

tree::TerminalNode* KoatParser::ComContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

std::vector<KoatParser::RhsContext *> KoatParser::ComContext::rhs() {
  return getRuleContexts<KoatParser::RhsContext>();
}

KoatParser::RhsContext* KoatParser::ComContext::rhs(size_t i) {
  return getRuleContext<KoatParser::RhsContext>(i);
}

std::vector<tree::TerminalNode *> KoatParser::ComContext::COMMA() {
  return getTokens(KoatParser::COMMA);
}

tree::TerminalNode* KoatParser::ComContext::COMMA(size_t i) {
  return getToken(KoatParser::COMMA, i);
}


size_t KoatParser::ComContext::getRuleIndex() const {
  return KoatParser::RuleCom;
}

void KoatParser::ComContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterCom(this);
}

void KoatParser::ComContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitCom(this);
}


antlrcpp::Any KoatParser::ComContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitCom(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::ComContext* KoatParser::com() {
  ComContext *_localctx = _tracker.createInstance<ComContext>(_ctx, getState());
  enterRule(_localctx, 18, KoatParser::RuleCom);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(105);
    match(KoatParser::COM);
    setState(106);
    match(KoatParser::LPAR);
    setState(115);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == KoatParser::ID) {
      setState(107);
      rhs();
      setState(112);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == KoatParser::COMMA) {
        setState(108);
        match(KoatParser::COMMA);
        setState(109);
        rhs();
        setState(114);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(117);
    match(KoatParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- RhsContext ------------------------------------------------------------------

KoatParser::RhsContext::RhsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::FsContext* KoatParser::RhsContext::fs() {
  return getRuleContext<KoatParser::FsContext>(0);
}

tree::TerminalNode* KoatParser::RhsContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

tree::TerminalNode* KoatParser::RhsContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

std::vector<KoatParser::ExprContext *> KoatParser::RhsContext::expr() {
  return getRuleContexts<KoatParser::ExprContext>();
}

KoatParser::ExprContext* KoatParser::RhsContext::expr(size_t i) {
  return getRuleContext<KoatParser::ExprContext>(i);
}

std::vector<tree::TerminalNode *> KoatParser::RhsContext::COMMA() {
  return getTokens(KoatParser::COMMA);
}

tree::TerminalNode* KoatParser::RhsContext::COMMA(size_t i) {
  return getToken(KoatParser::COMMA, i);
}


size_t KoatParser::RhsContext::getRuleIndex() const {
  return KoatParser::RuleRhs;
}

void KoatParser::RhsContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterRhs(this);
}

void KoatParser::RhsContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitRhs(this);
}


antlrcpp::Any KoatParser::RhsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitRhs(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::RhsContext* KoatParser::rhs() {
  RhsContext *_localctx = _tracker.createInstance<RhsContext>(_ctx, getState());
  enterRule(_localctx, 20, KoatParser::RuleRhs);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(119);
    fs();
    setState(120);
    match(KoatParser::LPAR);
    setState(129);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & ((1ULL << KoatParser::MINUS)
      | (1ULL << KoatParser::LPAR)
      | (1ULL << KoatParser::ID)
      | (1ULL << KoatParser::INT))) != 0)) {
      setState(121);
      expr(0);
      setState(126);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == KoatParser::COMMA) {
        setState(122);
        match(KoatParser::COMMA);
        setState(123);
        expr(0);
        setState(128);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(131);
    match(KoatParser::RPAR);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ToContext ------------------------------------------------------------------

KoatParser::ToContext::ToContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::ToContext::TO() {
  return getToken(KoatParser::TO, 0);
}

tree::TerminalNode* KoatParser::ToContext::MINUS() {
  return getToken(KoatParser::MINUS, 0);
}

tree::TerminalNode* KoatParser::ToContext::LCURL() {
  return getToken(KoatParser::LCURL, 0);
}

KoatParser::LbContext* KoatParser::ToContext::lb() {
  return getRuleContext<KoatParser::LbContext>(0);
}

tree::TerminalNode* KoatParser::ToContext::COMMA() {
  return getToken(KoatParser::COMMA, 0);
}

KoatParser::UbContext* KoatParser::ToContext::ub() {
  return getRuleContext<KoatParser::UbContext>(0);
}

tree::TerminalNode* KoatParser::ToContext::RCURL() {
  return getToken(KoatParser::RCURL, 0);
}

tree::TerminalNode* KoatParser::ToContext::GT() {
  return getToken(KoatParser::GT, 0);
}


size_t KoatParser::ToContext::getRuleIndex() const {
  return KoatParser::RuleTo;
}

void KoatParser::ToContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterTo(this);
}

void KoatParser::ToContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitTo(this);
}


antlrcpp::Any KoatParser::ToContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitTo(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::ToContext* KoatParser::to() {
  ToContext *_localctx = _tracker.createInstance<ToContext>(_ctx, getState());
  enterRule(_localctx, 22, KoatParser::RuleTo);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    setState(148);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 10, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(133);
      match(KoatParser::TO);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(134);
      match(KoatParser::MINUS);
      setState(135);
      match(KoatParser::LCURL);
      setState(136);
      lb();
      setState(137);
      match(KoatParser::COMMA);
      setState(138);
      ub();
      setState(139);
      match(KoatParser::RCURL);
      setState(140);
      match(KoatParser::GT);
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(142);
      match(KoatParser::MINUS);
      setState(143);
      match(KoatParser::LCURL);
      setState(144);
      lb();
      setState(145);
      match(KoatParser::RCURL);
      setState(146);
      match(KoatParser::GT);
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

//----------------- LbContext ------------------------------------------------------------------

KoatParser::LbContext::LbContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::ExprContext* KoatParser::LbContext::expr() {
  return getRuleContext<KoatParser::ExprContext>(0);
}


size_t KoatParser::LbContext::getRuleIndex() const {
  return KoatParser::RuleLb;
}

void KoatParser::LbContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLb(this);
}

void KoatParser::LbContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLb(this);
}


antlrcpp::Any KoatParser::LbContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitLb(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::LbContext* KoatParser::lb() {
  LbContext *_localctx = _tracker.createInstance<LbContext>(_ctx, getState());
  enterRule(_localctx, 24, KoatParser::RuleLb);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(150);
    expr(0);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- UbContext ------------------------------------------------------------------

KoatParser::UbContext::UbContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::ExprContext* KoatParser::UbContext::expr() {
  return getRuleContext<KoatParser::ExprContext>(0);
}


size_t KoatParser::UbContext::getRuleIndex() const {
  return KoatParser::RuleUb;
}

void KoatParser::UbContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterUb(this);
}

void KoatParser::UbContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitUb(this);
}


antlrcpp::Any KoatParser::UbContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitUb(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::UbContext* KoatParser::ub() {
  UbContext *_localctx = _tracker.createInstance<UbContext>(_ctx, getState());
  enterRule(_localctx, 26, KoatParser::RuleUb);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(152);
    expr(0);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CondContext ------------------------------------------------------------------

KoatParser::CondContext::CondContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::CondContext::CONDSEP() {
  return getToken(KoatParser::CONDSEP, 0);
}

KoatParser::FormulaContext* KoatParser::CondContext::formula() {
  return getRuleContext<KoatParser::FormulaContext>(0);
}

tree::TerminalNode* KoatParser::CondContext::LBRACK() {
  return getToken(KoatParser::LBRACK, 0);
}

tree::TerminalNode* KoatParser::CondContext::RBRACK() {
  return getToken(KoatParser::RBRACK, 0);
}


size_t KoatParser::CondContext::getRuleIndex() const {
  return KoatParser::RuleCond;
}

void KoatParser::CondContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterCond(this);
}

void KoatParser::CondContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitCond(this);
}


antlrcpp::Any KoatParser::CondContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitCond(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::CondContext* KoatParser::cond() {
  CondContext *_localctx = _tracker.createInstance<CondContext>(_ctx, getState());
  enterRule(_localctx, 28, KoatParser::RuleCond);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    setState(160);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case KoatParser::CONDSEP: {
        enterOuterAlt(_localctx, 1);
        setState(154);
        match(KoatParser::CONDSEP);
        setState(155);
        formula(0);
        break;
      }

      case KoatParser::LBRACK: {
        enterOuterAlt(_localctx, 2);
        setState(156);
        match(KoatParser::LBRACK);
        setState(157);
        formula(0);
        setState(158);
        match(KoatParser::RBRACK);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprContext ------------------------------------------------------------------

KoatParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::VarContext* KoatParser::ExprContext::var() {
  return getRuleContext<KoatParser::VarContext>(0);
}

tree::TerminalNode* KoatParser::ExprContext::INT() {
  return getToken(KoatParser::INT, 0);
}

tree::TerminalNode* KoatParser::ExprContext::MINUS() {
  return getToken(KoatParser::MINUS, 0);
}

std::vector<KoatParser::ExprContext *> KoatParser::ExprContext::expr() {
  return getRuleContexts<KoatParser::ExprContext>();
}

KoatParser::ExprContext* KoatParser::ExprContext::expr(size_t i) {
  return getRuleContext<KoatParser::ExprContext>(i);
}

tree::TerminalNode* KoatParser::ExprContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

tree::TerminalNode* KoatParser::ExprContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

KoatParser::BinopContext* KoatParser::ExprContext::binop() {
  return getRuleContext<KoatParser::BinopContext>(0);
}


size_t KoatParser::ExprContext::getRuleIndex() const {
  return KoatParser::RuleExpr;
}

void KoatParser::ExprContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterExpr(this);
}

void KoatParser::ExprContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitExpr(this);
}


antlrcpp::Any KoatParser::ExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitExpr(this);
  else
    return visitor->visitChildren(this);
}


KoatParser::ExprContext* KoatParser::expr() {
   return expr(0);
}

KoatParser::ExprContext* KoatParser::expr(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  KoatParser::ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, parentState);
  KoatParser::ExprContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 30;
  enterRecursionRule(_localctx, 30, KoatParser::RuleExpr, precedence);

    

  auto onExit = finally([=] {
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(171);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case KoatParser::ID: {
        setState(163);
        var();
        break;
      }

      case KoatParser::INT: {
        setState(164);
        match(KoatParser::INT);
        break;
      }

      case KoatParser::MINUS: {
        setState(165);
        match(KoatParser::MINUS);
        setState(166);
        expr(3);
        break;
      }

      case KoatParser::LPAR: {
        setState(167);
        match(KoatParser::LPAR);
        setState(168);
        expr(0);
        setState(169);
        match(KoatParser::RPAR);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    _ctx->stop = _input->LT(-1);
    setState(179);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 13, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<ExprContext>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleExpr);
        setState(173);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(174);
        binop();
        setState(175);
        expr(3); 
      }
      setState(181);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 13, _ctx);
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

KoatParser::BinopContext::BinopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::BinopContext::EXP() {
  return getToken(KoatParser::EXP, 0);
}

tree::TerminalNode* KoatParser::BinopContext::TIMES() {
  return getToken(KoatParser::TIMES, 0);
}

tree::TerminalNode* KoatParser::BinopContext::PLUS() {
  return getToken(KoatParser::PLUS, 0);
}

tree::TerminalNode* KoatParser::BinopContext::MINUS() {
  return getToken(KoatParser::MINUS, 0);
}


size_t KoatParser::BinopContext::getRuleIndex() const {
  return KoatParser::RuleBinop;
}

void KoatParser::BinopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBinop(this);
}

void KoatParser::BinopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBinop(this);
}


antlrcpp::Any KoatParser::BinopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitBinop(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::BinopContext* KoatParser::binop() {
  BinopContext *_localctx = _tracker.createInstance<BinopContext>(_ctx, getState());
  enterRule(_localctx, 32, KoatParser::RuleBinop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(182);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & ((1ULL << KoatParser::PLUS)
      | (1ULL << KoatParser::MINUS)
      | (1ULL << KoatParser::TIMES)
      | (1ULL << KoatParser::EXP))) != 0))) {
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

KoatParser::FormulaContext::FormulaContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

KoatParser::LitContext* KoatParser::FormulaContext::lit() {
  return getRuleContext<KoatParser::LitContext>(0);
}

tree::TerminalNode* KoatParser::FormulaContext::LPAR() {
  return getToken(KoatParser::LPAR, 0);
}

std::vector<KoatParser::FormulaContext *> KoatParser::FormulaContext::formula() {
  return getRuleContexts<KoatParser::FormulaContext>();
}

KoatParser::FormulaContext* KoatParser::FormulaContext::formula(size_t i) {
  return getRuleContext<KoatParser::FormulaContext>(i);
}

tree::TerminalNode* KoatParser::FormulaContext::RPAR() {
  return getToken(KoatParser::RPAR, 0);
}

KoatParser::BoolopContext* KoatParser::FormulaContext::boolop() {
  return getRuleContext<KoatParser::BoolopContext>(0);
}


size_t KoatParser::FormulaContext::getRuleIndex() const {
  return KoatParser::RuleFormula;
}

void KoatParser::FormulaContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterFormula(this);
}

void KoatParser::FormulaContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitFormula(this);
}


antlrcpp::Any KoatParser::FormulaContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitFormula(this);
  else
    return visitor->visitChildren(this);
}


KoatParser::FormulaContext* KoatParser::formula() {
   return formula(0);
}

KoatParser::FormulaContext* KoatParser::formula(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  KoatParser::FormulaContext *_localctx = _tracker.createInstance<FormulaContext>(_ctx, parentState);
  KoatParser::FormulaContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 34;
  enterRecursionRule(_localctx, 34, KoatParser::RuleFormula, precedence);

    

  auto onExit = finally([=] {
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(190);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 14, _ctx)) {
    case 1: {
      setState(185);
      lit();
      break;
    }

    case 2: {
      setState(186);
      match(KoatParser::LPAR);
      setState(187);
      formula(0);
      setState(188);
      match(KoatParser::RPAR);
      break;
    }

    }
    _ctx->stop = _input->LT(-1);
    setState(198);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 15, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        _localctx = _tracker.createInstance<FormulaContext>(parentContext, parentState);
        pushNewRecursionContext(_localctx, startState, RuleFormula);
        setState(192);

        if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
        setState(193);
        boolop();
        setState(194);
        formula(3); 
      }
      setState(200);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 15, _ctx);
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

KoatParser::LitContext::LitContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<KoatParser::ExprContext *> KoatParser::LitContext::expr() {
  return getRuleContexts<KoatParser::ExprContext>();
}

KoatParser::ExprContext* KoatParser::LitContext::expr(size_t i) {
  return getRuleContext<KoatParser::ExprContext>(i);
}

KoatParser::RelopContext* KoatParser::LitContext::relop() {
  return getRuleContext<KoatParser::RelopContext>(0);
}


size_t KoatParser::LitContext::getRuleIndex() const {
  return KoatParser::RuleLit;
}

void KoatParser::LitContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterLit(this);
}

void KoatParser::LitContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitLit(this);
}


antlrcpp::Any KoatParser::LitContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitLit(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::LitContext* KoatParser::lit() {
  LitContext *_localctx = _tracker.createInstance<LitContext>(_ctx, getState());
  enterRule(_localctx, 36, KoatParser::RuleLit);

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(201);
    expr(0);
    setState(202);
    relop();
    setState(203);
    expr(0);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BoolopContext ------------------------------------------------------------------

KoatParser::BoolopContext::BoolopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::BoolopContext::AND() {
  return getToken(KoatParser::AND, 0);
}

tree::TerminalNode* KoatParser::BoolopContext::OR() {
  return getToken(KoatParser::OR, 0);
}


size_t KoatParser::BoolopContext::getRuleIndex() const {
  return KoatParser::RuleBoolop;
}

void KoatParser::BoolopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterBoolop(this);
}

void KoatParser::BoolopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitBoolop(this);
}


antlrcpp::Any KoatParser::BoolopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitBoolop(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::BoolopContext* KoatParser::boolop() {
  BoolopContext *_localctx = _tracker.createInstance<BoolopContext>(_ctx, getState());
  enterRule(_localctx, 38, KoatParser::RuleBoolop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(205);
    _la = _input->LA(1);
    if (!(_la == KoatParser::AND

    || _la == KoatParser::OR)) {
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

KoatParser::RelopContext::RelopContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* KoatParser::RelopContext::LT() {
  return getToken(KoatParser::LT, 0);
}

tree::TerminalNode* KoatParser::RelopContext::LEQ() {
  return getToken(KoatParser::LEQ, 0);
}

tree::TerminalNode* KoatParser::RelopContext::EQ() {
  return getToken(KoatParser::EQ, 0);
}

tree::TerminalNode* KoatParser::RelopContext::GT() {
  return getToken(KoatParser::GT, 0);
}

tree::TerminalNode* KoatParser::RelopContext::GEQ() {
  return getToken(KoatParser::GEQ, 0);
}

tree::TerminalNode* KoatParser::RelopContext::NEQ() {
  return getToken(KoatParser::NEQ, 0);
}


size_t KoatParser::RelopContext::getRuleIndex() const {
  return KoatParser::RuleRelop;
}

void KoatParser::RelopContext::enterRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->enterRelop(this);
}

void KoatParser::RelopContext::exitRule(tree::ParseTreeListener *listener) {
  auto parserListener = dynamic_cast<KoatListener *>(listener);
  if (parserListener != nullptr)
    parserListener->exitRelop(this);
}


antlrcpp::Any KoatParser::RelopContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<KoatVisitor*>(visitor))
    return parserVisitor->visitRelop(this);
  else
    return visitor->visitChildren(this);
}

KoatParser::RelopContext* KoatParser::relop() {
  RelopContext *_localctx = _tracker.createInstance<RelopContext>(_ctx, getState());
  enterRule(_localctx, 40, KoatParser::RuleRelop);
  size_t _la = 0;

  auto onExit = finally([=] {
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(207);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & ((1ULL << KoatParser::LT)
      | (1ULL << KoatParser::LEQ)
      | (1ULL << KoatParser::EQ)
      | (1ULL << KoatParser::NEQ)
      | (1ULL << KoatParser::GEQ)
      | (1ULL << KoatParser::GT))) != 0))) {
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

bool KoatParser::sempred(RuleContext *context, size_t ruleIndex, size_t predicateIndex) {
  switch (ruleIndex) {
    case 15: return exprSempred(dynamic_cast<ExprContext *>(context), predicateIndex);
    case 17: return formulaSempred(dynamic_cast<FormulaContext *>(context), predicateIndex);

  default:
    break;
  }
  return true;
}

bool KoatParser::exprSempred(ExprContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 0: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

bool KoatParser::formulaSempred(FormulaContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 1: return precpred(_ctx, 2);

  default:
    break;
  }
  return true;
}

// Static vars and initialization.
std::vector<dfa::DFA> KoatParser::_decisionToDFA;
atn::PredictionContextCache KoatParser::_sharedContextCache;

// We own the ATN which in turn owns the ATN states.
atn::ATN KoatParser::_atn;
std::vector<uint16_t> KoatParser::_serializedATN;

std::vector<std::string> KoatParser::_ruleNames = {
  "main", "fs", "var", "goal", "start", "vardecl", "transs", "trans", "lhs", 
  "com", "rhs", "to", "lb", "ub", "cond", "expr", "binop", "formula", "lit", 
  "boolop", "relop"
};

std::vector<std::string> KoatParser::_literalNames = {
  "", "", "'GOAL'", "'COMPLEXITY'", "'TERMINATION'", "'STARTTERM'", "'FUNCTIONSYMBOLS'", 
  "'VAR'", "'RULES'", "'+'", "'-'", "'*'", "", "'('", "')'", "']'", "'['", 
  "'{'", "'}'", "'->'", "','", "", "", "'<'", "'<='", "'=='", "'!='", "'>='", 
  "'>'", "':|:'"
};

std::vector<std::string> KoatParser::_symbolicNames = {
  "", "COM", "GOAL", "CPX", "TERM", "START", "FS", "VAR", "RULES", "PLUS", 
  "MINUS", "TIMES", "EXP", "LPAR", "RPAR", "RBRACK", "LBRACK", "LCURL", 
  "RCURL", "TO", "COMMA", "AND", "OR", "LT", "LEQ", "EQ", "NEQ", "GEQ", 
  "GT", "CONDSEP", "ID", "INT", "WS", "COMMENT"
};

dfa::Vocabulary KoatParser::_vocabulary(_literalNames, _symbolicNames);

std::vector<std::string> KoatParser::_tokenNames;

KoatParser::Initializer::Initializer() {
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
    0x3, 0x23, 0xd4, 0x4, 0x2, 0x9, 0x2, 0x4, 0x3, 0x9, 0x3, 0x4, 0x4, 0x9, 
    0x4, 0x4, 0x5, 0x9, 0x5, 0x4, 0x6, 0x9, 0x6, 0x4, 0x7, 0x9, 0x7, 0x4, 
    0x8, 0x9, 0x8, 0x4, 0x9, 0x9, 0x9, 0x4, 0xa, 0x9, 0xa, 0x4, 0xb, 0x9, 
    0xb, 0x4, 0xc, 0x9, 0xc, 0x4, 0xd, 0x9, 0xd, 0x4, 0xe, 0x9, 0xe, 0x4, 
    0xf, 0x9, 0xf, 0x4, 0x10, 0x9, 0x10, 0x4, 0x11, 0x9, 0x11, 0x4, 0x12, 
    0x9, 0x12, 0x4, 0x13, 0x9, 0x13, 0x4, 0x14, 0x9, 0x14, 0x4, 0x15, 0x9, 
    0x15, 0x4, 0x16, 0x9, 0x16, 0x3, 0x2, 0x5, 0x2, 0x2e, 0xa, 0x2, 0x3, 
    0x2, 0x3, 0x2, 0x3, 0x2, 0x3, 0x2, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 
    0x4, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x6, 0x3, 
    0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 
    0x7, 0x3, 0x7, 0x3, 0x7, 0x6, 0x7, 0x48, 0xa, 0x7, 0xd, 0x7, 0xe, 0x7, 
    0x49, 0x3, 0x7, 0x3, 0x7, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x7, 0x8, 0x51, 
    0xa, 0x8, 0xc, 0x8, 0xe, 0x8, 0x54, 0xb, 0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 
    0x9, 0x3, 0x9, 0x3, 0x9, 0x3, 0x9, 0x5, 0x9, 0x5c, 0xa, 0x9, 0x3, 0xa, 
    0x3, 0xa, 0x3, 0xa, 0x3, 0xa, 0x3, 0xa, 0x7, 0xa, 0x63, 0xa, 0xa, 0xc, 
    0xa, 0xe, 0xa, 0x66, 0xb, 0xa, 0x5, 0xa, 0x68, 0xa, 0xa, 0x3, 0xa, 0x3, 
    0xa, 0x3, 0xb, 0x3, 0xb, 0x3, 0xb, 0x3, 0xb, 0x3, 0xb, 0x7, 0xb, 0x71, 
    0xa, 0xb, 0xc, 0xb, 0xe, 0xb, 0x74, 0xb, 0xb, 0x5, 0xb, 0x76, 0xa, 0xb, 
    0x3, 0xb, 0x3, 0xb, 0x3, 0xc, 0x3, 0xc, 0x3, 0xc, 0x3, 0xc, 0x3, 0xc, 
    0x7, 0xc, 0x7f, 0xa, 0xc, 0xc, 0xc, 0xe, 0xc, 0x82, 0xb, 0xc, 0x5, 0xc, 
    0x84, 0xa, 0xc, 0x3, 0xc, 0x3, 0xc, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 
    0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 
    0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x3, 0xd, 0x5, 0xd, 0x97, 0xa, 0xd, 
    0x3, 0xe, 0x3, 0xe, 0x3, 0xf, 0x3, 0xf, 0x3, 0x10, 0x3, 0x10, 0x3, 0x10, 
    0x3, 0x10, 0x3, 0x10, 0x3, 0x10, 0x5, 0x10, 0xa3, 0xa, 0x10, 0x3, 0x11, 
    0x3, 0x11, 0x3, 0x11, 0x3, 0x11, 0x3, 0x11, 0x3, 0x11, 0x3, 0x11, 0x3, 
    0x11, 0x3, 0x11, 0x5, 0x11, 0xae, 0xa, 0x11, 0x3, 0x11, 0x3, 0x11, 0x3, 
    0x11, 0x3, 0x11, 0x7, 0x11, 0xb4, 0xa, 0x11, 0xc, 0x11, 0xe, 0x11, 0xb7, 
    0xb, 0x11, 0x3, 0x12, 0x3, 0x12, 0x3, 0x13, 0x3, 0x13, 0x3, 0x13, 0x3, 
    0x13, 0x3, 0x13, 0x3, 0x13, 0x5, 0x13, 0xc1, 0xa, 0x13, 0x3, 0x13, 0x3, 
    0x13, 0x3, 0x13, 0x3, 0x13, 0x7, 0x13, 0xc7, 0xa, 0x13, 0xc, 0x13, 0xe, 
    0x13, 0xca, 0xb, 0x13, 0x3, 0x14, 0x3, 0x14, 0x3, 0x14, 0x3, 0x14, 0x3, 
    0x15, 0x3, 0x15, 0x3, 0x16, 0x3, 0x16, 0x3, 0x16, 0x2, 0x4, 0x20, 0x24, 
    0x17, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, 0x10, 0x12, 0x14, 0x16, 0x18, 
    0x1a, 0x1c, 0x1e, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2, 0x6, 0x3, 
    0x2, 0x5, 0x6, 0x3, 0x2, 0xb, 0xe, 0x3, 0x2, 0x17, 0x18, 0x3, 0x2, 0x19, 
    0x1e, 0x2, 0xd1, 0x2, 0x2d, 0x3, 0x2, 0x2, 0x2, 0x4, 0x33, 0x3, 0x2, 
    0x2, 0x2, 0x6, 0x35, 0x3, 0x2, 0x2, 0x2, 0x8, 0x37, 0x3, 0x2, 0x2, 0x2, 
    0xa, 0x3c, 0x3, 0x2, 0x2, 0x2, 0xc, 0x44, 0x3, 0x2, 0x2, 0x2, 0xe, 0x4d, 
    0x3, 0x2, 0x2, 0x2, 0x10, 0x57, 0x3, 0x2, 0x2, 0x2, 0x12, 0x5d, 0x3, 
    0x2, 0x2, 0x2, 0x14, 0x6b, 0x3, 0x2, 0x2, 0x2, 0x16, 0x79, 0x3, 0x2, 
    0x2, 0x2, 0x18, 0x96, 0x3, 0x2, 0x2, 0x2, 0x1a, 0x98, 0x3, 0x2, 0x2, 
    0x2, 0x1c, 0x9a, 0x3, 0x2, 0x2, 0x2, 0x1e, 0xa2, 0x3, 0x2, 0x2, 0x2, 
    0x20, 0xad, 0x3, 0x2, 0x2, 0x2, 0x22, 0xb8, 0x3, 0x2, 0x2, 0x2, 0x24, 
    0xc0, 0x3, 0x2, 0x2, 0x2, 0x26, 0xcb, 0x3, 0x2, 0x2, 0x2, 0x28, 0xcf, 
    0x3, 0x2, 0x2, 0x2, 0x2a, 0xd1, 0x3, 0x2, 0x2, 0x2, 0x2c, 0x2e, 0x5, 
    0x8, 0x5, 0x2, 0x2d, 0x2c, 0x3, 0x2, 0x2, 0x2, 0x2d, 0x2e, 0x3, 0x2, 
    0x2, 0x2, 0x2e, 0x2f, 0x3, 0x2, 0x2, 0x2, 0x2f, 0x30, 0x5, 0xa, 0x6, 
    0x2, 0x30, 0x31, 0x5, 0xc, 0x7, 0x2, 0x31, 0x32, 0x5, 0xe, 0x8, 0x2, 
    0x32, 0x3, 0x3, 0x2, 0x2, 0x2, 0x33, 0x34, 0x7, 0x20, 0x2, 0x2, 0x34, 
    0x5, 0x3, 0x2, 0x2, 0x2, 0x35, 0x36, 0x7, 0x20, 0x2, 0x2, 0x36, 0x7, 
    0x3, 0x2, 0x2, 0x2, 0x37, 0x38, 0x7, 0xf, 0x2, 0x2, 0x38, 0x39, 0x7, 
    0x4, 0x2, 0x2, 0x39, 0x3a, 0x9, 0x2, 0x2, 0x2, 0x3a, 0x3b, 0x7, 0x10, 
    0x2, 0x2, 0x3b, 0x9, 0x3, 0x2, 0x2, 0x2, 0x3c, 0x3d, 0x7, 0xf, 0x2, 
    0x2, 0x3d, 0x3e, 0x7, 0x7, 0x2, 0x2, 0x3e, 0x3f, 0x7, 0xf, 0x2, 0x2, 
    0x3f, 0x40, 0x7, 0x8, 0x2, 0x2, 0x40, 0x41, 0x5, 0x4, 0x3, 0x2, 0x41, 
    0x42, 0x7, 0x10, 0x2, 0x2, 0x42, 0x43, 0x7, 0x10, 0x2, 0x2, 0x43, 0xb, 
    0x3, 0x2, 0x2, 0x2, 0x44, 0x45, 0x7, 0xf, 0x2, 0x2, 0x45, 0x47, 0x7, 
    0x9, 0x2, 0x2, 0x46, 0x48, 0x7, 0x20, 0x2, 0x2, 0x47, 0x46, 0x3, 0x2, 
    0x2, 0x2, 0x48, 0x49, 0x3, 0x2, 0x2, 0x2, 0x49, 0x47, 0x3, 0x2, 0x2, 
    0x2, 0x49, 0x4a, 0x3, 0x2, 0x2, 0x2, 0x4a, 0x4b, 0x3, 0x2, 0x2, 0x2, 
    0x4b, 0x4c, 0x7, 0x10, 0x2, 0x2, 0x4c, 0xd, 0x3, 0x2, 0x2, 0x2, 0x4d, 
    0x4e, 0x7, 0xf, 0x2, 0x2, 0x4e, 0x52, 0x7, 0xa, 0x2, 0x2, 0x4f, 0x51, 
    0x5, 0x10, 0x9, 0x2, 0x50, 0x4f, 0x3, 0x2, 0x2, 0x2, 0x51, 0x54, 0x3, 
    0x2, 0x2, 0x2, 0x52, 0x50, 0x3, 0x2, 0x2, 0x2, 0x52, 0x53, 0x3, 0x2, 
    0x2, 0x2, 0x53, 0x55, 0x3, 0x2, 0x2, 0x2, 0x54, 0x52, 0x3, 0x2, 0x2, 
    0x2, 0x55, 0x56, 0x7, 0x10, 0x2, 0x2, 0x56, 0xf, 0x3, 0x2, 0x2, 0x2, 
    0x57, 0x58, 0x5, 0x12, 0xa, 0x2, 0x58, 0x59, 0x5, 0x18, 0xd, 0x2, 0x59, 
    0x5b, 0x5, 0x14, 0xb, 0x2, 0x5a, 0x5c, 0x5, 0x1e, 0x10, 0x2, 0x5b, 0x5a, 
    0x3, 0x2, 0x2, 0x2, 0x5b, 0x5c, 0x3, 0x2, 0x2, 0x2, 0x5c, 0x11, 0x3, 
    0x2, 0x2, 0x2, 0x5d, 0x5e, 0x5, 0x4, 0x3, 0x2, 0x5e, 0x67, 0x7, 0xf, 
    0x2, 0x2, 0x5f, 0x64, 0x5, 0x6, 0x4, 0x2, 0x60, 0x61, 0x7, 0x16, 0x2, 
    0x2, 0x61, 0x63, 0x5, 0x6, 0x4, 0x2, 0x62, 0x60, 0x3, 0x2, 0x2, 0x2, 
    0x63, 0x66, 0x3, 0x2, 0x2, 0x2, 0x64, 0x62, 0x3, 0x2, 0x2, 0x2, 0x64, 
    0x65, 0x3, 0x2, 0x2, 0x2, 0x65, 0x68, 0x3, 0x2, 0x2, 0x2, 0x66, 0x64, 
    0x3, 0x2, 0x2, 0x2, 0x67, 0x5f, 0x3, 0x2, 0x2, 0x2, 0x67, 0x68, 0x3, 
    0x2, 0x2, 0x2, 0x68, 0x69, 0x3, 0x2, 0x2, 0x2, 0x69, 0x6a, 0x7, 0x10, 
    0x2, 0x2, 0x6a, 0x13, 0x3, 0x2, 0x2, 0x2, 0x6b, 0x6c, 0x7, 0x3, 0x2, 
    0x2, 0x6c, 0x75, 0x7, 0xf, 0x2, 0x2, 0x6d, 0x72, 0x5, 0x16, 0xc, 0x2, 
    0x6e, 0x6f, 0x7, 0x16, 0x2, 0x2, 0x6f, 0x71, 0x5, 0x16, 0xc, 0x2, 0x70, 
    0x6e, 0x3, 0x2, 0x2, 0x2, 0x71, 0x74, 0x3, 0x2, 0x2, 0x2, 0x72, 0x70, 
    0x3, 0x2, 0x2, 0x2, 0x72, 0x73, 0x3, 0x2, 0x2, 0x2, 0x73, 0x76, 0x3, 
    0x2, 0x2, 0x2, 0x74, 0x72, 0x3, 0x2, 0x2, 0x2, 0x75, 0x6d, 0x3, 0x2, 
    0x2, 0x2, 0x75, 0x76, 0x3, 0x2, 0x2, 0x2, 0x76, 0x77, 0x3, 0x2, 0x2, 
    0x2, 0x77, 0x78, 0x7, 0x10, 0x2, 0x2, 0x78, 0x15, 0x3, 0x2, 0x2, 0x2, 
    0x79, 0x7a, 0x5, 0x4, 0x3, 0x2, 0x7a, 0x83, 0x7, 0xf, 0x2, 0x2, 0x7b, 
    0x80, 0x5, 0x20, 0x11, 0x2, 0x7c, 0x7d, 0x7, 0x16, 0x2, 0x2, 0x7d, 0x7f, 
    0x5, 0x20, 0x11, 0x2, 0x7e, 0x7c, 0x3, 0x2, 0x2, 0x2, 0x7f, 0x82, 0x3, 
    0x2, 0x2, 0x2, 0x80, 0x7e, 0x3, 0x2, 0x2, 0x2, 0x80, 0x81, 0x3, 0x2, 
    0x2, 0x2, 0x81, 0x84, 0x3, 0x2, 0x2, 0x2, 0x82, 0x80, 0x3, 0x2, 0x2, 
    0x2, 0x83, 0x7b, 0x3, 0x2, 0x2, 0x2, 0x83, 0x84, 0x3, 0x2, 0x2, 0x2, 
    0x84, 0x85, 0x3, 0x2, 0x2, 0x2, 0x85, 0x86, 0x7, 0x10, 0x2, 0x2, 0x86, 
    0x17, 0x3, 0x2, 0x2, 0x2, 0x87, 0x97, 0x7, 0x15, 0x2, 0x2, 0x88, 0x89, 
    0x7, 0xc, 0x2, 0x2, 0x89, 0x8a, 0x7, 0x13, 0x2, 0x2, 0x8a, 0x8b, 0x5, 
    0x1a, 0xe, 0x2, 0x8b, 0x8c, 0x7, 0x16, 0x2, 0x2, 0x8c, 0x8d, 0x5, 0x1c, 
    0xf, 0x2, 0x8d, 0x8e, 0x7, 0x14, 0x2, 0x2, 0x8e, 0x8f, 0x7, 0x1e, 0x2, 
    0x2, 0x8f, 0x97, 0x3, 0x2, 0x2, 0x2, 0x90, 0x91, 0x7, 0xc, 0x2, 0x2, 
    0x91, 0x92, 0x7, 0x13, 0x2, 0x2, 0x92, 0x93, 0x5, 0x1a, 0xe, 0x2, 0x93, 
    0x94, 0x7, 0x14, 0x2, 0x2, 0x94, 0x95, 0x7, 0x1e, 0x2, 0x2, 0x95, 0x97, 
    0x3, 0x2, 0x2, 0x2, 0x96, 0x87, 0x3, 0x2, 0x2, 0x2, 0x96, 0x88, 0x3, 
    0x2, 0x2, 0x2, 0x96, 0x90, 0x3, 0x2, 0x2, 0x2, 0x97, 0x19, 0x3, 0x2, 
    0x2, 0x2, 0x98, 0x99, 0x5, 0x20, 0x11, 0x2, 0x99, 0x1b, 0x3, 0x2, 0x2, 
    0x2, 0x9a, 0x9b, 0x5, 0x20, 0x11, 0x2, 0x9b, 0x1d, 0x3, 0x2, 0x2, 0x2, 
    0x9c, 0x9d, 0x7, 0x1f, 0x2, 0x2, 0x9d, 0xa3, 0x5, 0x24, 0x13, 0x2, 0x9e, 
    0x9f, 0x7, 0x12, 0x2, 0x2, 0x9f, 0xa0, 0x5, 0x24, 0x13, 0x2, 0xa0, 0xa1, 
    0x7, 0x11, 0x2, 0x2, 0xa1, 0xa3, 0x3, 0x2, 0x2, 0x2, 0xa2, 0x9c, 0x3, 
    0x2, 0x2, 0x2, 0xa2, 0x9e, 0x3, 0x2, 0x2, 0x2, 0xa3, 0x1f, 0x3, 0x2, 
    0x2, 0x2, 0xa4, 0xa5, 0x8, 0x11, 0x1, 0x2, 0xa5, 0xae, 0x5, 0x6, 0x4, 
    0x2, 0xa6, 0xae, 0x7, 0x21, 0x2, 0x2, 0xa7, 0xa8, 0x7, 0xc, 0x2, 0x2, 
    0xa8, 0xae, 0x5, 0x20, 0x11, 0x5, 0xa9, 0xaa, 0x7, 0xf, 0x2, 0x2, 0xaa, 
    0xab, 0x5, 0x20, 0x11, 0x2, 0xab, 0xac, 0x7, 0x10, 0x2, 0x2, 0xac, 0xae, 
    0x3, 0x2, 0x2, 0x2, 0xad, 0xa4, 0x3, 0x2, 0x2, 0x2, 0xad, 0xa6, 0x3, 
    0x2, 0x2, 0x2, 0xad, 0xa7, 0x3, 0x2, 0x2, 0x2, 0xad, 0xa9, 0x3, 0x2, 
    0x2, 0x2, 0xae, 0xb5, 0x3, 0x2, 0x2, 0x2, 0xaf, 0xb0, 0xc, 0x4, 0x2, 
    0x2, 0xb0, 0xb1, 0x5, 0x22, 0x12, 0x2, 0xb1, 0xb2, 0x5, 0x20, 0x11, 
    0x5, 0xb2, 0xb4, 0x3, 0x2, 0x2, 0x2, 0xb3, 0xaf, 0x3, 0x2, 0x2, 0x2, 
    0xb4, 0xb7, 0x3, 0x2, 0x2, 0x2, 0xb5, 0xb3, 0x3, 0x2, 0x2, 0x2, 0xb5, 
    0xb6, 0x3, 0x2, 0x2, 0x2, 0xb6, 0x21, 0x3, 0x2, 0x2, 0x2, 0xb7, 0xb5, 
    0x3, 0x2, 0x2, 0x2, 0xb8, 0xb9, 0x9, 0x3, 0x2, 0x2, 0xb9, 0x23, 0x3, 
    0x2, 0x2, 0x2, 0xba, 0xbb, 0x8, 0x13, 0x1, 0x2, 0xbb, 0xc1, 0x5, 0x26, 
    0x14, 0x2, 0xbc, 0xbd, 0x7, 0xf, 0x2, 0x2, 0xbd, 0xbe, 0x5, 0x24, 0x13, 
    0x2, 0xbe, 0xbf, 0x7, 0x10, 0x2, 0x2, 0xbf, 0xc1, 0x3, 0x2, 0x2, 0x2, 
    0xc0, 0xba, 0x3, 0x2, 0x2, 0x2, 0xc0, 0xbc, 0x3, 0x2, 0x2, 0x2, 0xc1, 
    0xc8, 0x3, 0x2, 0x2, 0x2, 0xc2, 0xc3, 0xc, 0x4, 0x2, 0x2, 0xc3, 0xc4, 
    0x5, 0x28, 0x15, 0x2, 0xc4, 0xc5, 0x5, 0x24, 0x13, 0x5, 0xc5, 0xc7, 
    0x3, 0x2, 0x2, 0x2, 0xc6, 0xc2, 0x3, 0x2, 0x2, 0x2, 0xc7, 0xca, 0x3, 
    0x2, 0x2, 0x2, 0xc8, 0xc6, 0x3, 0x2, 0x2, 0x2, 0xc8, 0xc9, 0x3, 0x2, 
    0x2, 0x2, 0xc9, 0x25, 0x3, 0x2, 0x2, 0x2, 0xca, 0xc8, 0x3, 0x2, 0x2, 
    0x2, 0xcb, 0xcc, 0x5, 0x20, 0x11, 0x2, 0xcc, 0xcd, 0x5, 0x2a, 0x16, 
    0x2, 0xcd, 0xce, 0x5, 0x20, 0x11, 0x2, 0xce, 0x27, 0x3, 0x2, 0x2, 0x2, 
    0xcf, 0xd0, 0x9, 0x4, 0x2, 0x2, 0xd0, 0x29, 0x3, 0x2, 0x2, 0x2, 0xd1, 
    0xd2, 0x9, 0x5, 0x2, 0x2, 0xd2, 0x2b, 0x3, 0x2, 0x2, 0x2, 0x12, 0x2d, 
    0x49, 0x52, 0x5b, 0x64, 0x67, 0x72, 0x75, 0x80, 0x83, 0x96, 0xa2, 0xad, 
    0xb5, 0xc0, 0xc8, 
  };

  atn::ATNDeserializer deserializer;
  _atn = deserializer.deserialize(_serializedATN);

  size_t count = _atn.getNumberOfDecisions();
  _decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    _decisionToDFA.emplace_back(_atn.getDecisionState(i), i);
  }
}

KoatParser::Initializer KoatParser::_init;
