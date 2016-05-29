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

#ifndef ITRS_H
#define ITRS_H

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <ginac/ginac.h>

#include "global.h"
#include "exceptions.h"
#include "expression.h"
#include "term.h"

/**
 * Represents a rule in an ITRS
 */
struct ITRSRule {
    TT::Expression lhs;
    TT::Expression rhs;
    TT::ExpressionVector guard;
    TT::Expression cost;
};

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

/**
 * Class for parsing an ITRS
 * @note variable names contain only alphanumeric characters and _
 */

class ITRS {
public:
    /**
     * Loads an ITRS data from the given file
     */
    static ITRS loadFromFile(const std::string &filename);

    //simple getters
    inline FunctionSymbolIndex getStartFunctionSymbol() const { return startFunctionSymbol; }
    inline std::string getFunctionSymbolName(FunctionSymbolIndex index) const { return functionSymbols[index]; }
    inline FunctionSymbolIndex getFunctionSymbolIndex(const std::string &name) const { return functionSymbolNameMap.at(name); }
    inline const std::vector<std::string>::size_type getFunctionSymbolCount() const { return functionSymbols.size(); }
    inline const std::vector<std::string>& getFunctionSymbolNames() const { return functionSymbols; }

    inline std::string getVariableName(VariableIndex index) const { return variables[index]; }
    inline ExprSymbol getGinacSymbol(VariableIndex index) const { return ginacSymbols[index]; }
    inline VariableIndex getVariableIndex(const std::string &name) const { return variableMap.at(name); }
    inline const std::vector<std::string>& getVariables() const { return variables; }
    inline ExprList getGinacVarList() const { return varSymbolList; }

    inline const std::vector<ITRSRule>& getITRSRules() const { return rules; }

    // helpers for variable handling
    VariableIndex addVariable(const std::string &name);
    VariableIndex addFreshVariable(const std::string &basename);
    std::string getFreshName(const std::string &baseName) const;

    // helpers for function symbol handling
    FunctionSymbolIndex addFunctionSymbol(const std::string &name);
    FunctionSymbolIndex addFreshFunctionSymbol(const std::string &basename);
    std::string getFreshFunctionSymbolName(const std::string &basename) const;

    /**
     * Prints this ITS contents in a readable but ugly format, for debugging only
     * @param s output stream to print to (e.g. cout)
     */
    virtual void print(std::ostream &os) const;

protected:
    ITRS() {}
    void load(const std::string &filename);
    EXCEPTION(FileError,CustomException);

private:
    void verifyFunctionSymbolArity();
    EXCEPTION(ArityMismatchException,CustomException);

    // parsing
    std::string escapeVariableName(const std::string &name);
    void parseRule(const std::string &line);
    void parseLeftHandSide(const std::string &lhs);
    void parseRightHandSide(const std::string &rhs);
    void parseCost(const std::string &cost);
    void parseGuard(const std::string &guard);

    // term parser
    EXCEPTION(UnexpectedSymbolException, CustomException);
    EXCEPTION(UnknownSymbolException, CustomException);
    EXCEPTION(UnknownVariableException, CustomException);
    EXCEPTION(UnexpectedEndOfTextException, CustomException);
    EXCEPTION(SyntaxErrorException, CustomException);
    TT::Expression parseTerm(const std::string &term);
    void nextSymbol();
    bool accept(Symbol sym);
    bool expect(Symbol sym);
    TT::Expression expression();
    TT::Expression term();
    TT::Expression factor();

private:
    static const std::set<char> specialCharsInVarNames;

    /* ITRS Data */
    std::vector<std::string> variables;
    std::vector<std::string> functionSymbols;
    std::vector<ITRSRule> rules;
    FunctionSymbolIndex startFunctionSymbol;

    /* for lookup efficiency */
    std::map<std::string,VariableIndex> variableMap;
    std::map<std::string,FunctionSymbolIndex> functionSymbolNameMap;

    /* needed since GiNaC::symbols must be referenced later on */
    /* NOTE: symbols with the same name are *NOT* identical to GiNaC */
    std::vector<ExprSymbol> ginacSymbols;
    ExprList varSymbolList;

    /* parser stuff */
    ITRSRule newRule;
    std::map<std::string,VariableIndex> knownVariables;
    bool nextSymbolCalledOnEmptyInput;
    std::string toParseReversed;
    std::string lastIdent;
    Symbol symbol;
};

#endif // ITRS_H