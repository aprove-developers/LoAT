/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#include "termparser.h"

#include <fstream>
#include <boost/algorithm/string.hpp>

using namespace parser;

using namespace std;
using namespace boost::algorithm;


const std::set<char> TermParser::specialCharsInVarNames = {'\'', '.', '_'};


TermParser::TermParser(const std::map<std::string, VariableIdx> &knownVariables, bool allowDivision)
 : knownVariables(knownVariables), allowDivision(allowDivision)
{}


TermPtr TermParser::parseTerm(const std::string &term) {
    debugTermParser("Now parsing term: " << term);
    toParseReversed = term;
    std::reverse(toParseReversed.begin(), toParseReversed.end());

    nextSymbolCalledOnEmptyInput = false;
    nextSymbol();
    return expression();
}


void TermParser::nextSymbol() {
    trim_right(toParseReversed);

    if (nextSymbolCalledOnEmptyInput) {
        throw UnexpectedEndOfTextException();
    }

    if (toParseReversed.empty()) {
        nextSymbolCalledOnEmptyInput = true;
        return;
    }

    char nextChar = toParseReversed.back();
    debugTermParser("nextSymbol read char: " << nextChar);

    if (isdigit(nextChar)) {
        lastIdent.clear();
        while (!toParseReversed.empty() && isdigit(toParseReversed.back())) {
            lastIdent += toParseReversed.back();
            toParseReversed.pop_back();
        }

        symbol = NUMBER;
        debugTermParser("nextSymbol found number: " << lastIdent);

    } else if (isalpha(nextChar)) {
        lastIdent.clear();
        while (!toParseReversed.empty()
               && (isalnum(toParseReversed.back()) || specialCharsInVarNames.count(toParseReversed.back()) == 1))
        {
            lastIdent += toParseReversed.back();
            toParseReversed.pop_back();
        }

        if (!toParseReversed.empty() && toParseReversed.back() == '(') {
            symbol = FUNCTIONSYMBOL;
            debugTermParser("nextSymbol found function symbol: " << lastIdent);

        } else {
            symbol = VARIABLE;
            debugTermParser("nextSymbol found variable: " << lastIdent);
        }

    } else {
        if (nextChar == '+') {
            symbol = PLUS;

        } else if (nextChar == '-') {
            symbol = MINUS;

        }  else if (nextChar == '*') {
            symbol = TIMES;

        }  else if (nextChar == '/') {
            symbol = SLASH;

        }  else if (nextChar == '^') {
            symbol = CIRCUMFLEX;

        } else if (nextChar == '(') {
            symbol = LPAREN;

        } else if (nextChar == ')') {
            symbol = RPAREN;

        } else if (nextChar == ',') {
            symbol = COMMA;

        } else {
            throw UnknownSymbolException("Unknown symbol: " + string(1, nextChar));
        }

        toParseReversed.pop_back();
        debugTermParser("[nextSymbol] found symbol");
    }
}


bool TermParser::accept(Symbol sym) {
    if (sym == symbol) {
        nextSymbol();
        return true;

    } else {
        return false;
    }
}


bool TermParser::expect(Symbol sym) {
    if (accept(sym)) {
        return true;
    } else {
        throw UnexpectedSymbolException();
    }
}


TermPtr TermParser::expression() {
    debugTermParser("parsing expression");
    bool negative = false;
    if (symbol == PLUS || symbol == MINUS) {
        negative = (symbol == MINUS);

        nextSymbol();
    }

    TermPtr result = term();
    if (negative) {
        TermPtr sign = make_shared<TermNumber>(-1);
        result = make_shared<TermBinOp>(sign, result, TermBinOp::Multiplication);
    }

    while (symbol == PLUS || symbol == MINUS) {
        negative = (symbol == MINUS);
        nextSymbol();

        TermPtr nextTerm = term();
        if (negative) {
            result = make_shared<TermBinOp>(result, nextTerm, TermBinOp::Subtraction);

        } else {
            result = make_shared<TermBinOp>(result, nextTerm, TermBinOp::Addition);
        }
    }

    return result;
}


TermPtr TermParser::term() {
    debugTermParser("parsing term");
    TermPtr result = factor();

    while (symbol == TIMES || symbol == SLASH || symbol == CIRCUMFLEX) {
        Symbol op = symbol;
        nextSymbol();

        if (op == TIMES) {
            result = make_shared<TermBinOp>(result, factor(), TermBinOp::Multiplication);

        } else if (op == SLASH) {
            if (allowDivision) {
                result = make_shared<TermBinOp>(result, factor(), TermBinOp::Division);
            } else {
                throw ForbiddenDivisionException("Division is not allowed in the input");
            }

        } else { // CIRCUMFLEX
            result = make_shared<TermBinOp>(result, factor(), TermBinOp::Power);
        }
    }

    return result;
}


TermPtr TermParser::factor() {
    debugTermParser("parsing factor");
    if (accept(FUNCTIONSYMBOL)) {
        string name = lastIdent;
        debugTermParser("parsing function symbol " << name);

        expect(LPAREN);

        vector<TermPtr> args;

        // check for empty argument list
        if (!accept(RPAREN)) {
            do {
                args.push_back(expression());
            } while (accept(COMMA));

            expect(RPAREN);
        }

        return make_shared<TermFunApp>(name, args);

    } else if (accept(VARIABLE)) {
        std::string name = lastIdent;
        debugTermParser("parsing variable " << name);

        auto it = knownVariables.find(name);
        if (it == knownVariables.end()) {
            debugTermParser("Ooops, " << name << " is a function symbol of arity 0");

            vector<TermPtr> args;
            return make_shared<TermFunApp>(name, args);
        }

        VariableIdx index = it->second;
        return make_shared<TermVariable>(index);

    } else if (accept(NUMBER)) {
        debugTermParser("parsing number " << lastIdent);

        GiNaC::numeric num(lastIdent.c_str());
        return make_shared<TermNumber>(num);

    } else if (accept(LPAREN)) {
        TermPtr result = expression();
        expect(RPAREN);
        return result;

    } else {
        throw SyntaxErrorException();
    }
}
