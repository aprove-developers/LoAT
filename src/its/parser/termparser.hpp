//
// Created by matt on 11.04.18.
//

#ifndef TERMPARSER_H
#define TERMPARSER_H

#include <string>

#include "../../its/itsproblem.hpp"
#include "../../util/exceptions.hpp"
#include "term.hpp"

namespace parser {

/**
 * A simple parser for terms (arithmetic expressions with function applications)
 */
class TermParser {
public:
    EXCEPTION(TermParserException, CustomException);

    EXCEPTION(UnexpectedSymbolException, TermParserException);
    EXCEPTION(UnknownSymbolException, TermParserException);
    EXCEPTION(UnknownVariableException, TermParserException);
    EXCEPTION(UnexpectedEndOfTextException, TermParserException);
    EXCEPTION(SyntaxErrorException, TermParserException);
    EXCEPTION(ForbiddenDivisionException, TermParserException);

    /**
     * Create a TermParser instance
     * @param knownVariables Maps variable names (unescaped) to the index of the corresponding variable
     * @param allowDivision Whether to allow (and parse) divisions. If false, ForbiddenDivisionException may be thrown
     */
    TermParser(const std::map<std::string, Var> &knownVariables, bool allowDivision);

    /**
     * Tries to parse the given string into a term. Throws exception on failure.
     * It is safe to call this method several times on a single TermParser instance.
     */
    TermPtr parseTerm(const std::string &s);

private:
    static const std::set<char> specialCharsInVarNames;

    enum Symbol {
        NUMBER,
        PLUS,
        MINUS,
        TIMES,
        SLASH,
        CIRCUMFLEX,
        FUNCTIONSYMBOL,
        VARIABLE,
        LPAREN,
        RPAREN,
        COMMA
    };

    void nextSymbol();
    bool accept(Symbol sym);
    bool expect(Symbol sym);
    TermPtr expression();
    TermPtr term();
    TermPtr factor();

    // settings
    const std::map<std::string, Var> &knownVariables;
    bool allowDivision;

    // parser state
    bool nextSymbolCalledOnEmptyInput;
    std::string toParseReversed;
    std::string lastIdent;
    Symbol symbol;
};

}

#endif //TERMPARSER_H
