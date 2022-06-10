
// Generated from Koat.g4 by ANTLR 4.7.2


#include "KoatLexer.h"


using namespace antlr4;


KoatLexer::KoatLexer(CharStream *input) : Lexer(input) {
  _interpreter = new atn::LexerATNSimulator(this, _atn, _decisionToDFA, _sharedContextCache);
}

KoatLexer::~KoatLexer() {
  delete _interpreter;
}

std::string KoatLexer::getGrammarFileName() const {
  return "Koat.g4";
}

const std::vector<std::string>& KoatLexer::getRuleNames() const {
  return _ruleNames;
}

const std::vector<std::string>& KoatLexer::getChannelNames() const {
  return _channelNames;
}

const std::vector<std::string>& KoatLexer::getModeNames() const {
  return _modeNames;
}

const std::vector<std::string>& KoatLexer::getTokenNames() const {
  return _tokenNames;
}

dfa::Vocabulary& KoatLexer::getVocabulary() const {
  return _vocabulary;
}

const std::vector<uint16_t> KoatLexer::getSerializedATN() const {
  return _serializedATN;
}

const atn::ATN& KoatLexer::getATN() const {
  return _atn;
}




// Static vars and initialization.
std::vector<dfa::DFA> KoatLexer::_decisionToDFA;
atn::PredictionContextCache KoatLexer::_sharedContextCache;

// We own the ATN which in turn owns the ATN states.
atn::ATN KoatLexer::_atn;
std::vector<uint16_t> KoatLexer::_serializedATN;

std::vector<std::string> KoatLexer::_ruleNames = {
  u8"GOAL", u8"CPX", u8"TERM", u8"START", u8"FS", u8"VAR", u8"RULES", u8"PLUS", 
  u8"MINUS", u8"TIMES", u8"EXP", u8"LPAR", u8"RPAR", u8"RBRACK", u8"LBRACK", 
  u8"LCURL", u8"RCURL", u8"TO", u8"COMMA", u8"AND", u8"OR", u8"LT", u8"LEQ", 
  u8"EQ", u8"GEQ", u8"GT", u8"CONDSEP", u8"ID", u8"INT", u8"WS", u8"COMMENT"
};

std::vector<std::string> KoatLexer::_channelNames = {
  "DEFAULT_TOKEN_CHANNEL", "HIDDEN"
};

std::vector<std::string> KoatLexer::_modeNames = {
  u8"DEFAULT_MODE"
};

std::vector<std::string> KoatLexer::_literalNames = {
  "", u8"'GOAL'", u8"'COMPLEXITY'", u8"'TERMINATION'", u8"'STARTTERM'", 
  u8"'FUNCTIONSYMBOLS'", u8"'VAR'", u8"'RULES'", u8"'+'", u8"'-'", u8"'*'", 
  "", u8"'('", u8"')'", u8"']'", u8"'['", u8"'{'", u8"'}'", u8"'->'", u8"','", 
  "", "", u8"'<'", u8"'<='", u8"'='", u8"'>='", u8"'>'", u8"':|:'"
};

std::vector<std::string> KoatLexer::_symbolicNames = {
  "", u8"GOAL", u8"CPX", u8"TERM", u8"START", u8"FS", u8"VAR", u8"RULES", 
  u8"PLUS", u8"MINUS", u8"TIMES", u8"EXP", u8"LPAR", u8"RPAR", u8"RBRACK", 
  u8"LBRACK", u8"LCURL", u8"RCURL", u8"TO", u8"COMMA", u8"AND", u8"OR", 
  u8"LT", u8"LEQ", u8"EQ", u8"GEQ", u8"GT", u8"CONDSEP", u8"ID", u8"INT", 
  u8"WS", u8"COMMENT"
};

dfa::Vocabulary KoatLexer::_vocabulary(_literalNames, _symbolicNames);

std::vector<std::string> KoatLexer::_tokenNames;

KoatLexer::Initializer::Initializer() {
  // This code could be in a static initializer lambda, but VS doesn't allow access to private class members from there.
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
    0x2, 0x21, 0xdc, 0x8, 0x1, 0x4, 0x2, 0x9, 0x2, 0x4, 0x3, 0x9, 0x3, 0x4, 
    0x4, 0x9, 0x4, 0x4, 0x5, 0x9, 0x5, 0x4, 0x6, 0x9, 0x6, 0x4, 0x7, 0x9, 
    0x7, 0x4, 0x8, 0x9, 0x8, 0x4, 0x9, 0x9, 0x9, 0x4, 0xa, 0x9, 0xa, 0x4, 
    0xb, 0x9, 0xb, 0x4, 0xc, 0x9, 0xc, 0x4, 0xd, 0x9, 0xd, 0x4, 0xe, 0x9, 
    0xe, 0x4, 0xf, 0x9, 0xf, 0x4, 0x10, 0x9, 0x10, 0x4, 0x11, 0x9, 0x11, 
    0x4, 0x12, 0x9, 0x12, 0x4, 0x13, 0x9, 0x13, 0x4, 0x14, 0x9, 0x14, 0x4, 
    0x15, 0x9, 0x15, 0x4, 0x16, 0x9, 0x16, 0x4, 0x17, 0x9, 0x17, 0x4, 0x18, 
    0x9, 0x18, 0x4, 0x19, 0x9, 0x19, 0x4, 0x1a, 0x9, 0x1a, 0x4, 0x1b, 0x9, 
    0x1b, 0x4, 0x1c, 0x9, 0x1c, 0x4, 0x1d, 0x9, 0x1d, 0x4, 0x1e, 0x9, 0x1e, 
    0x4, 0x1f, 0x9, 0x1f, 0x4, 0x20, 0x9, 0x20, 0x3, 0x2, 0x3, 0x2, 0x3, 
    0x2, 0x3, 0x2, 0x3, 0x2, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
    0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 
    0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x4, 0x3, 0x5, 0x3, 0x5, 0x3, 
    0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 0x5, 0x3, 
    0x5, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 
    0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 
    0x6, 0x3, 0x6, 0x3, 0x6, 0x3, 0x7, 0x3, 0x7, 0x3, 0x7, 0x3, 0x7, 0x3, 
    0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 0x8, 0x3, 0x9, 0x3, 
    0x9, 0x3, 0xa, 0x3, 0xa, 0x3, 0xb, 0x3, 0xb, 0x3, 0xc, 0x3, 0xc, 0x3, 
    0xc, 0x5, 0xc, 0x8b, 0xa, 0xc, 0x3, 0xd, 0x3, 0xd, 0x3, 0xe, 0x3, 0xe, 
    0x3, 0xf, 0x3, 0xf, 0x3, 0x10, 0x3, 0x10, 0x3, 0x11, 0x3, 0x11, 0x3, 
    0x12, 0x3, 0x12, 0x3, 0x13, 0x3, 0x13, 0x3, 0x13, 0x3, 0x14, 0x3, 0x14, 
    0x3, 0x15, 0x3, 0x15, 0x3, 0x15, 0x3, 0x15, 0x5, 0x15, 0xa2, 0xa, 0x15, 
    0x3, 0x16, 0x3, 0x16, 0x3, 0x16, 0x3, 0x16, 0x5, 0x16, 0xa8, 0xa, 0x16, 
    0x3, 0x17, 0x3, 0x17, 0x3, 0x18, 0x3, 0x18, 0x3, 0x18, 0x3, 0x19, 0x3, 
    0x19, 0x3, 0x1a, 0x3, 0x1a, 0x3, 0x1a, 0x3, 0x1b, 0x3, 0x1b, 0x3, 0x1c, 
    0x3, 0x1c, 0x3, 0x1c, 0x3, 0x1c, 0x3, 0x1d, 0x3, 0x1d, 0x7, 0x1d, 0xbc, 
    0xa, 0x1d, 0xc, 0x1d, 0xe, 0x1d, 0xbf, 0xb, 0x1d, 0x3, 0x1e, 0x6, 0x1e, 
    0xc2, 0xa, 0x1e, 0xd, 0x1e, 0xe, 0x1e, 0xc3, 0x3, 0x1f, 0x6, 0x1f, 0xc7, 
    0xa, 0x1f, 0xd, 0x1f, 0xe, 0x1f, 0xc8, 0x3, 0x1f, 0x3, 0x1f, 0x3, 0x20, 
    0x3, 0x20, 0x7, 0x20, 0xcf, 0xa, 0x20, 0xc, 0x20, 0xe, 0x20, 0xd2, 0xb, 
    0x20, 0x3, 0x20, 0x5, 0x20, 0xd5, 0xa, 0x20, 0x3, 0x20, 0x3, 0x20, 0x5, 
    0x20, 0xd9, 0xa, 0x20, 0x3, 0x20, 0x3, 0x20, 0x3, 0xd0, 0x2, 0x21, 0x3, 
    0x3, 0x5, 0x4, 0x7, 0x5, 0x9, 0x6, 0xb, 0x7, 0xd, 0x8, 0xf, 0x9, 0x11, 
    0xa, 0x13, 0xb, 0x15, 0xc, 0x17, 0xd, 0x19, 0xe, 0x1b, 0xf, 0x1d, 0x10, 
    0x1f, 0x11, 0x21, 0x12, 0x23, 0x13, 0x25, 0x14, 0x27, 0x15, 0x29, 0x16, 
    0x2b, 0x17, 0x2d, 0x18, 0x2f, 0x19, 0x31, 0x1a, 0x33, 0x1b, 0x35, 0x1c, 
    0x37, 0x1d, 0x39, 0x1e, 0x3b, 0x1f, 0x3d, 0x20, 0x3f, 0x21, 0x3, 0x2, 
    0x5, 0x5, 0x2, 0x43, 0x5c, 0x61, 0x61, 0x63, 0x7c, 0x8, 0x2, 0x29, 0x29, 
    0x30, 0x30, 0x32, 0x3b, 0x43, 0x5c, 0x61, 0x61, 0x63, 0x7c, 0x5, 0x2, 
    0xb, 0xc, 0xf, 0xf, 0x22, 0x22, 0x2, 0xe4, 0x2, 0x3, 0x3, 0x2, 0x2, 
    0x2, 0x2, 0x5, 0x3, 0x2, 0x2, 0x2, 0x2, 0x7, 0x3, 0x2, 0x2, 0x2, 0x2, 
    0x9, 0x3, 0x2, 0x2, 0x2, 0x2, 0xb, 0x3, 0x2, 0x2, 0x2, 0x2, 0xd, 0x3, 
    0x2, 0x2, 0x2, 0x2, 0xf, 0x3, 0x2, 0x2, 0x2, 0x2, 0x11, 0x3, 0x2, 0x2, 
    0x2, 0x2, 0x13, 0x3, 0x2, 0x2, 0x2, 0x2, 0x15, 0x3, 0x2, 0x2, 0x2, 0x2, 
    0x17, 0x3, 0x2, 0x2, 0x2, 0x2, 0x19, 0x3, 0x2, 0x2, 0x2, 0x2, 0x1b, 
    0x3, 0x2, 0x2, 0x2, 0x2, 0x1d, 0x3, 0x2, 0x2, 0x2, 0x2, 0x1f, 0x3, 0x2, 
    0x2, 0x2, 0x2, 0x21, 0x3, 0x2, 0x2, 0x2, 0x2, 0x23, 0x3, 0x2, 0x2, 0x2, 
    0x2, 0x25, 0x3, 0x2, 0x2, 0x2, 0x2, 0x27, 0x3, 0x2, 0x2, 0x2, 0x2, 0x29, 
    0x3, 0x2, 0x2, 0x2, 0x2, 0x2b, 0x3, 0x2, 0x2, 0x2, 0x2, 0x2d, 0x3, 0x2, 
    0x2, 0x2, 0x2, 0x2f, 0x3, 0x2, 0x2, 0x2, 0x2, 0x31, 0x3, 0x2, 0x2, 0x2, 
    0x2, 0x33, 0x3, 0x2, 0x2, 0x2, 0x2, 0x35, 0x3, 0x2, 0x2, 0x2, 0x2, 0x37, 
    0x3, 0x2, 0x2, 0x2, 0x2, 0x39, 0x3, 0x2, 0x2, 0x2, 0x2, 0x3b, 0x3, 0x2, 
    0x2, 0x2, 0x2, 0x3d, 0x3, 0x2, 0x2, 0x2, 0x2, 0x3f, 0x3, 0x2, 0x2, 0x2, 
    0x3, 0x41, 0x3, 0x2, 0x2, 0x2, 0x5, 0x46, 0x3, 0x2, 0x2, 0x2, 0x7, 0x51, 
    0x3, 0x2, 0x2, 0x2, 0x9, 0x5d, 0x3, 0x2, 0x2, 0x2, 0xb, 0x67, 0x3, 0x2, 
    0x2, 0x2, 0xd, 0x77, 0x3, 0x2, 0x2, 0x2, 0xf, 0x7b, 0x3, 0x2, 0x2, 0x2, 
    0x11, 0x81, 0x3, 0x2, 0x2, 0x2, 0x13, 0x83, 0x3, 0x2, 0x2, 0x2, 0x15, 
    0x85, 0x3, 0x2, 0x2, 0x2, 0x17, 0x8a, 0x3, 0x2, 0x2, 0x2, 0x19, 0x8c, 
    0x3, 0x2, 0x2, 0x2, 0x1b, 0x8e, 0x3, 0x2, 0x2, 0x2, 0x1d, 0x90, 0x3, 
    0x2, 0x2, 0x2, 0x1f, 0x92, 0x3, 0x2, 0x2, 0x2, 0x21, 0x94, 0x3, 0x2, 
    0x2, 0x2, 0x23, 0x96, 0x3, 0x2, 0x2, 0x2, 0x25, 0x98, 0x3, 0x2, 0x2, 
    0x2, 0x27, 0x9b, 0x3, 0x2, 0x2, 0x2, 0x29, 0xa1, 0x3, 0x2, 0x2, 0x2, 
    0x2b, 0xa7, 0x3, 0x2, 0x2, 0x2, 0x2d, 0xa9, 0x3, 0x2, 0x2, 0x2, 0x2f, 
    0xab, 0x3, 0x2, 0x2, 0x2, 0x31, 0xae, 0x3, 0x2, 0x2, 0x2, 0x33, 0xb0, 
    0x3, 0x2, 0x2, 0x2, 0x35, 0xb3, 0x3, 0x2, 0x2, 0x2, 0x37, 0xb5, 0x3, 
    0x2, 0x2, 0x2, 0x39, 0xb9, 0x3, 0x2, 0x2, 0x2, 0x3b, 0xc1, 0x3, 0x2, 
    0x2, 0x2, 0x3d, 0xc6, 0x3, 0x2, 0x2, 0x2, 0x3f, 0xcc, 0x3, 0x2, 0x2, 
    0x2, 0x41, 0x42, 0x7, 0x49, 0x2, 0x2, 0x42, 0x43, 0x7, 0x51, 0x2, 0x2, 
    0x43, 0x44, 0x7, 0x43, 0x2, 0x2, 0x44, 0x45, 0x7, 0x4e, 0x2, 0x2, 0x45, 
    0x4, 0x3, 0x2, 0x2, 0x2, 0x46, 0x47, 0x7, 0x45, 0x2, 0x2, 0x47, 0x48, 
    0x7, 0x51, 0x2, 0x2, 0x48, 0x49, 0x7, 0x4f, 0x2, 0x2, 0x49, 0x4a, 0x7, 
    0x52, 0x2, 0x2, 0x4a, 0x4b, 0x7, 0x4e, 0x2, 0x2, 0x4b, 0x4c, 0x7, 0x47, 
    0x2, 0x2, 0x4c, 0x4d, 0x7, 0x5a, 0x2, 0x2, 0x4d, 0x4e, 0x7, 0x4b, 0x2, 
    0x2, 0x4e, 0x4f, 0x7, 0x56, 0x2, 0x2, 0x4f, 0x50, 0x7, 0x5b, 0x2, 0x2, 
    0x50, 0x6, 0x3, 0x2, 0x2, 0x2, 0x51, 0x52, 0x7, 0x56, 0x2, 0x2, 0x52, 
    0x53, 0x7, 0x47, 0x2, 0x2, 0x53, 0x54, 0x7, 0x54, 0x2, 0x2, 0x54, 0x55, 
    0x7, 0x4f, 0x2, 0x2, 0x55, 0x56, 0x7, 0x4b, 0x2, 0x2, 0x56, 0x57, 0x7, 
    0x50, 0x2, 0x2, 0x57, 0x58, 0x7, 0x43, 0x2, 0x2, 0x58, 0x59, 0x7, 0x56, 
    0x2, 0x2, 0x59, 0x5a, 0x7, 0x4b, 0x2, 0x2, 0x5a, 0x5b, 0x7, 0x51, 0x2, 
    0x2, 0x5b, 0x5c, 0x7, 0x50, 0x2, 0x2, 0x5c, 0x8, 0x3, 0x2, 0x2, 0x2, 
    0x5d, 0x5e, 0x7, 0x55, 0x2, 0x2, 0x5e, 0x5f, 0x7, 0x56, 0x2, 0x2, 0x5f, 
    0x60, 0x7, 0x43, 0x2, 0x2, 0x60, 0x61, 0x7, 0x54, 0x2, 0x2, 0x61, 0x62, 
    0x7, 0x56, 0x2, 0x2, 0x62, 0x63, 0x7, 0x56, 0x2, 0x2, 0x63, 0x64, 0x7, 
    0x47, 0x2, 0x2, 0x64, 0x65, 0x7, 0x54, 0x2, 0x2, 0x65, 0x66, 0x7, 0x4f, 
    0x2, 0x2, 0x66, 0xa, 0x3, 0x2, 0x2, 0x2, 0x67, 0x68, 0x7, 0x48, 0x2, 
    0x2, 0x68, 0x69, 0x7, 0x57, 0x2, 0x2, 0x69, 0x6a, 0x7, 0x50, 0x2, 0x2, 
    0x6a, 0x6b, 0x7, 0x45, 0x2, 0x2, 0x6b, 0x6c, 0x7, 0x56, 0x2, 0x2, 0x6c, 
    0x6d, 0x7, 0x4b, 0x2, 0x2, 0x6d, 0x6e, 0x7, 0x51, 0x2, 0x2, 0x6e, 0x6f, 
    0x7, 0x50, 0x2, 0x2, 0x6f, 0x70, 0x7, 0x55, 0x2, 0x2, 0x70, 0x71, 0x7, 
    0x5b, 0x2, 0x2, 0x71, 0x72, 0x7, 0x4f, 0x2, 0x2, 0x72, 0x73, 0x7, 0x44, 
    0x2, 0x2, 0x73, 0x74, 0x7, 0x51, 0x2, 0x2, 0x74, 0x75, 0x7, 0x4e, 0x2, 
    0x2, 0x75, 0x76, 0x7, 0x55, 0x2, 0x2, 0x76, 0xc, 0x3, 0x2, 0x2, 0x2, 
    0x77, 0x78, 0x7, 0x58, 0x2, 0x2, 0x78, 0x79, 0x7, 0x43, 0x2, 0x2, 0x79, 
    0x7a, 0x7, 0x54, 0x2, 0x2, 0x7a, 0xe, 0x3, 0x2, 0x2, 0x2, 0x7b, 0x7c, 
    0x7, 0x54, 0x2, 0x2, 0x7c, 0x7d, 0x7, 0x57, 0x2, 0x2, 0x7d, 0x7e, 0x7, 
    0x4e, 0x2, 0x2, 0x7e, 0x7f, 0x7, 0x47, 0x2, 0x2, 0x7f, 0x80, 0x7, 0x55, 
    0x2, 0x2, 0x80, 0x10, 0x3, 0x2, 0x2, 0x2, 0x81, 0x82, 0x7, 0x2d, 0x2, 
    0x2, 0x82, 0x12, 0x3, 0x2, 0x2, 0x2, 0x83, 0x84, 0x7, 0x2f, 0x2, 0x2, 
    0x84, 0x14, 0x3, 0x2, 0x2, 0x2, 0x85, 0x86, 0x7, 0x2c, 0x2, 0x2, 0x86, 
    0x16, 0x3, 0x2, 0x2, 0x2, 0x87, 0x8b, 0x7, 0x60, 0x2, 0x2, 0x88, 0x89, 
    0x7, 0x2c, 0x2, 0x2, 0x89, 0x8b, 0x7, 0x2c, 0x2, 0x2, 0x8a, 0x87, 0x3, 
    0x2, 0x2, 0x2, 0x8a, 0x88, 0x3, 0x2, 0x2, 0x2, 0x8b, 0x18, 0x3, 0x2, 
    0x2, 0x2, 0x8c, 0x8d, 0x7, 0x2a, 0x2, 0x2, 0x8d, 0x1a, 0x3, 0x2, 0x2, 
    0x2, 0x8e, 0x8f, 0x7, 0x2b, 0x2, 0x2, 0x8f, 0x1c, 0x3, 0x2, 0x2, 0x2, 
    0x90, 0x91, 0x7, 0x5f, 0x2, 0x2, 0x91, 0x1e, 0x3, 0x2, 0x2, 0x2, 0x92, 
    0x93, 0x7, 0x5d, 0x2, 0x2, 0x93, 0x20, 0x3, 0x2, 0x2, 0x2, 0x94, 0x95, 
    0x7, 0x7d, 0x2, 0x2, 0x95, 0x22, 0x3, 0x2, 0x2, 0x2, 0x96, 0x97, 0x7, 
    0x7f, 0x2, 0x2, 0x97, 0x24, 0x3, 0x2, 0x2, 0x2, 0x98, 0x99, 0x7, 0x2f, 
    0x2, 0x2, 0x99, 0x9a, 0x7, 0x40, 0x2, 0x2, 0x9a, 0x26, 0x3, 0x2, 0x2, 
    0x2, 0x9b, 0x9c, 0x7, 0x2e, 0x2, 0x2, 0x9c, 0x28, 0x3, 0x2, 0x2, 0x2, 
    0x9d, 0x9e, 0x7, 0x31, 0x2, 0x2, 0x9e, 0xa2, 0x7, 0x5e, 0x2, 0x2, 0x9f, 
    0xa0, 0x7, 0x28, 0x2, 0x2, 0xa0, 0xa2, 0x7, 0x28, 0x2, 0x2, 0xa1, 0x9d, 
    0x3, 0x2, 0x2, 0x2, 0xa1, 0x9f, 0x3, 0x2, 0x2, 0x2, 0xa2, 0x2a, 0x3, 
    0x2, 0x2, 0x2, 0xa3, 0xa4, 0x7, 0x5e, 0x2, 0x2, 0xa4, 0xa8, 0x7, 0x31, 
    0x2, 0x2, 0xa5, 0xa6, 0x7, 0x7e, 0x2, 0x2, 0xa6, 0xa8, 0x7, 0x7e, 0x2, 
    0x2, 0xa7, 0xa3, 0x3, 0x2, 0x2, 0x2, 0xa7, 0xa5, 0x3, 0x2, 0x2, 0x2, 
    0xa8, 0x2c, 0x3, 0x2, 0x2, 0x2, 0xa9, 0xaa, 0x7, 0x3e, 0x2, 0x2, 0xaa, 
    0x2e, 0x3, 0x2, 0x2, 0x2, 0xab, 0xac, 0x7, 0x3e, 0x2, 0x2, 0xac, 0xad, 
    0x7, 0x3f, 0x2, 0x2, 0xad, 0x30, 0x3, 0x2, 0x2, 0x2, 0xae, 0xaf, 0x7, 
    0x3f, 0x2, 0x2, 0xaf, 0x32, 0x3, 0x2, 0x2, 0x2, 0xb0, 0xb1, 0x7, 0x40, 
    0x2, 0x2, 0xb1, 0xb2, 0x7, 0x3f, 0x2, 0x2, 0xb2, 0x34, 0x3, 0x2, 0x2, 
    0x2, 0xb3, 0xb4, 0x7, 0x40, 0x2, 0x2, 0xb4, 0x36, 0x3, 0x2, 0x2, 0x2, 
    0xb5, 0xb6, 0x7, 0x3c, 0x2, 0x2, 0xb6, 0xb7, 0x7, 0x7e, 0x2, 0x2, 0xb7, 
    0xb8, 0x7, 0x3c, 0x2, 0x2, 0xb8, 0x38, 0x3, 0x2, 0x2, 0x2, 0xb9, 0xbd, 
    0x9, 0x2, 0x2, 0x2, 0xba, 0xbc, 0x9, 0x3, 0x2, 0x2, 0xbb, 0xba, 0x3, 
    0x2, 0x2, 0x2, 0xbc, 0xbf, 0x3, 0x2, 0x2, 0x2, 0xbd, 0xbb, 0x3, 0x2, 
    0x2, 0x2, 0xbd, 0xbe, 0x3, 0x2, 0x2, 0x2, 0xbe, 0x3a, 0x3, 0x2, 0x2, 
    0x2, 0xbf, 0xbd, 0x3, 0x2, 0x2, 0x2, 0xc0, 0xc2, 0x4, 0x32, 0x3b, 0x2, 
    0xc1, 0xc0, 0x3, 0x2, 0x2, 0x2, 0xc2, 0xc3, 0x3, 0x2, 0x2, 0x2, 0xc3, 
    0xc1, 0x3, 0x2, 0x2, 0x2, 0xc3, 0xc4, 0x3, 0x2, 0x2, 0x2, 0xc4, 0x3c, 
    0x3, 0x2, 0x2, 0x2, 0xc5, 0xc7, 0x9, 0x4, 0x2, 0x2, 0xc6, 0xc5, 0x3, 
    0x2, 0x2, 0x2, 0xc7, 0xc8, 0x3, 0x2, 0x2, 0x2, 0xc8, 0xc6, 0x3, 0x2, 
    0x2, 0x2, 0xc8, 0xc9, 0x3, 0x2, 0x2, 0x2, 0xc9, 0xca, 0x3, 0x2, 0x2, 
    0x2, 0xca, 0xcb, 0x8, 0x1f, 0x2, 0x2, 0xcb, 0x3e, 0x3, 0x2, 0x2, 0x2, 
    0xcc, 0xd0, 0x7, 0x25, 0x2, 0x2, 0xcd, 0xcf, 0xb, 0x2, 0x2, 0x2, 0xce, 
    0xcd, 0x3, 0x2, 0x2, 0x2, 0xcf, 0xd2, 0x3, 0x2, 0x2, 0x2, 0xd0, 0xd1, 
    0x3, 0x2, 0x2, 0x2, 0xd0, 0xce, 0x3, 0x2, 0x2, 0x2, 0xd1, 0xd8, 0x3, 
    0x2, 0x2, 0x2, 0xd2, 0xd0, 0x3, 0x2, 0x2, 0x2, 0xd3, 0xd5, 0x7, 0xf, 
    0x2, 0x2, 0xd4, 0xd3, 0x3, 0x2, 0x2, 0x2, 0xd4, 0xd5, 0x3, 0x2, 0x2, 
    0x2, 0xd5, 0xd6, 0x3, 0x2, 0x2, 0x2, 0xd6, 0xd9, 0x7, 0xc, 0x2, 0x2, 
    0xd7, 0xd9, 0x7, 0xf, 0x2, 0x2, 0xd8, 0xd4, 0x3, 0x2, 0x2, 0x2, 0xd8, 
    0xd7, 0x3, 0x2, 0x2, 0x2, 0xd9, 0xda, 0x3, 0x2, 0x2, 0x2, 0xda, 0xdb, 
    0x8, 0x20, 0x2, 0x2, 0xdb, 0x40, 0x3, 0x2, 0x2, 0x2, 0xc, 0x2, 0x8a, 
    0xa1, 0xa7, 0xbd, 0xc3, 0xc8, 0xd0, 0xd4, 0xd8, 0x3, 0x8, 0x2, 0x2, 
  };

  atn::ATNDeserializer deserializer;
  _atn = deserializer.deserialize(_serializedATN);

  size_t count = _atn.getNumberOfDecisions();
  _decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    _decisionToDFA.emplace_back(_atn.getDecisionState(i), i);
  }
}

KoatLexer::Initializer KoatLexer::_init;
