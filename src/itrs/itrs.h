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


namespace ITRS {

//typedefs for readability
typedef int TermIndex;
typedef std::vector<Expression> GuardList;
typedef std::map<VariableIndex,Expression> UpdateMap;


/**
 * Represents a rule in an ITS
 */
struct Rule {
    FunctionSymbolIndex lhs;
    TermIndex rhs;
    GuardList guard;
    Expression cost;
};

class VarSubVisitor : public ITRS::TermTree::Visitor {
public:
    VarSubVisitor(const std::map<VariableIndex,VariableIndex> &sub)
        : sub(sub) {
    }

    virtual void visitVariable(VariableIndex &variable) {
        if (sub.count(variable) == 1) {
            variable = sub.at(variable);
        }
    }

private:
    const std::map<VariableIndex,VariableIndex> &sub;
};

enum Symbol {
    NUMBER,
    PLUS,
    MINUS,
    TIMES,
    SLASH,
    FUNCTIONSYMBOL,
    VARIABLE,
    LPAREN,
    RPAREN,
    COMMA
};

/**
 * Datatype representing an ITS problem as defined in the sample files
 * @note variable names contain only alphanumeric characters and _
 */

class Problem {
    friend class Preprocessor; //allow direct access for the preprocessing

private:
    Problem() {}

public:
    /**
     * Loads the ITS data from the given file
     */
    static Problem loadFromFile(const std::string &filename);
    EXCEPTION(FileError,CustomException);

    //simple getters
    inline FunctionSymbolIndex getStartTerm() const { return startTerm; }
    inline std::shared_ptr<TermTree> getTerm(TermIndex idx) const { return terms[idx]; }
    inline TermIndex getTermCount() const { return terms.size(); }

    inline const std::vector<Rule>& getRules() const { return rules; }

    inline std::string getVarname(VariableIndex idx) const { return vars[idx]; }
    inline VariableIndex getVarindex(std::string name) const { return varMap.at(name); }

    inline const std::set<VariableIndex>& getFreeVars() const { return freeVars; }
    inline bool isFreeVar(VariableIndex idx) const { return freeVars.count(idx) > 0; }
    bool isFreeVar(const ExprSymbol &var) const;

    inline ExprSymbol getGinacSymbol(VariableIndex idx) const { return varSymbols[idx]; }
    inline ExprList getGinacVarList() const { return varSymbolList; }

    /**
     * Adds a new fresh variable based on the given name
     * @return the VariableIndex of the newly added variable
     */
    VariableIndex addFreshVariable(std::string basename, bool free = false);

    /**
     * Generates a fresh (unused) GiNaC symbol, but does _not_ add it to the ITS variables
     * @return the newly created symbol (_not_ associated with a variable index!)
     */
    ExprSymbol getFreshSymbol(std::string basename) const;

    /**
     * Prints this ITS contents in a readable but ugly format, for debugging only
     * @param s output stream to print to (e.g. cout)
     */
    void print(std::ostream &s) const;

private:
    //helpers for variable handling
    VariableIndex addVariable(std::string name);
    std::string getFreshName(std::string basename) const;

    //applies replacement map escapeSymbols to the given string (modified by reference)
    void substituteVarnames(std::string &line) const;

    void parseRule(const std::string &line);
    void parseLhs(std::string &lhs);
    void parseRhs(std::string &rhs);
    void parseCost(std::string &cost);
    void parseGuard(std::string &guard);
    bool replaceUnboundedWithFresh(const ExprSymbolSet &checkSymbols);

    // term parser
    EXCEPTION(UnexpectedSymbolException, CustomException);
    EXCEPTION(UnknownSymbolException, CustomException);
    EXCEPTION(UnknownVariableException, CustomException);
    EXCEPTION(UnexpectedEndOfTextException, CustomException);
    EXCEPTION(SyntaxErrorException, CustomException);
    std::shared_ptr<TermTree> parseTerm(const std::string &term);
    void nextSymbol();
    bool accept(Symbol sym);
    bool expect(Symbol sym);
    std::shared_ptr<TermTree> expression();
    std::shared_ptr<TermTree> term();
    std::shared_ptr<TermTree> factor();

    void printLhs(FunctionSymbolIndex fun, std::ostream &os) const;

private:
    /* ITS Data */
    std::vector<std::string> vars;
    std::set<VariableIndex> freeVars;
    std::vector<std::string> functionSymbols;
    std::map<FunctionSymbolIndex,std::vector<VariableIndex>> functionSymbolVars;
    std::vector<std::shared_ptr<TermTree>> terms;
    std::vector<Rule> rules;
    FunctionSymbolIndex startTerm;

    /* for lookup efficiency */
    std::map<std::string,VariableIndex> varMap;
    std::map<std::string,FunctionSymbolIndex> functionSymbolMap;

    /* needed since GiNaC::symbols must be referenced later on */
    /* NOTE: symbols with the same name are *NOT* identical to GiNaC */
    std::vector<ExprSymbol> varSymbols;
    ExprList varSymbolList;

    std::map<std::string,std::string> escapeSymbols;

    /* parser stuff */
    Rule newRule;
    GiNaC::exmap symbolSubs;
    ExprSymbolSet boundSymbols;
    std::map<std::string,VariableIndex> knownVars;
    bool nextSymbolCalledOnEmptyInput;
    std::string toParseReversed;
    std::string lastIdent;
    Symbol symbol;
};

} // namespace ITRS
#endif // ITRS_H