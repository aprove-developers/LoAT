
// Generated from Koat.g4 by ANTLR 4.7.2

#pragma once


#include "antlr4-runtime.h"




class  KoatLexer : public antlr4::Lexer {
public:
  enum {
    GOAL = 1, CPX = 2, TERM = 3, START = 4, FS = 5, VAR = 6, RULES = 7, 
    PLUS = 8, MINUS = 9, TIMES = 10, EXP = 11, LPAR = 12, RPAR = 13, RBRACK = 14, 
    LBRACK = 15, LCURL = 16, RCURL = 17, TO = 18, COMMA = 19, AND = 20, 
    OR = 21, LT = 22, LEQ = 23, EQ = 24, GEQ = 25, GT = 26, CONDSEP = 27, 
    ID = 28, INT = 29, WS = 30, COMMENT = 31
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

