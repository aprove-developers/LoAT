
// Generated from Koat.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"




class  KoatLexer : public antlr4::Lexer {
public:
  enum {
    COM = 1, GOAL = 2, CPX = 3, TERM = 4, START = 5, FS = 6, VAR = 7, RULES = 8, 
    PLUS = 9, MINUS = 10, TIMES = 11, EXP = 12, LPAR = 13, RPAR = 14, RBRACK = 15, 
    LBRACK = 16, LCURL = 17, RCURL = 18, TO = 19, COMMA = 20, AND = 21, 
    OR = 22, LT = 23, LEQ = 24, EQ = 25, NEQ = 26, GEQ = 27, GT = 28, CONDSEP = 29, 
    ID = 30, INT = 31, WS = 32, COMMENT = 33
  };

  KoatLexer(antlr4::CharStream *input);
  ~KoatLexer();

  virtual std::string getGrammarFileName() const override;
  virtual const std::vector<std::string>& getRuleNames() const override;

  virtual const std::vector<std::string>& getChannelNames() const override;
  virtual const std::vector<std::string>& getModeNames() const override;
  virtual const std::vector<std::string>& getTokenNames() const override; // deprecated, use vocabulary instead
  virtual antlr4::dfa::Vocabulary& getVocabulary() const override;

  virtual const std::vector<uint16_t> getSerializedATN() const override;
  virtual const antlr4::atn::ATN& getATN() const override;

private:
  static std::vector<antlr4::dfa::DFA> _decisionToDFA;
  static antlr4::atn::PredictionContextCache _sharedContextCache;
  static std::vector<std::string> _ruleNames;
  static std::vector<std::string> _tokenNames;
  static std::vector<std::string> _channelNames;
  static std::vector<std::string> _modeNames;

  static std::vector<std::string> _literalNames;
  static std::vector<std::string> _symbolicNames;
  static antlr4::dfa::Vocabulary _vocabulary;
  static antlr4::atn::ATN _atn;
  static std::vector<uint16_t> _serializedATN;


  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

  struct Initializer {
    Initializer();
  };
  static Initializer _init;
};

