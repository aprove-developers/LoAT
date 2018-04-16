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

#include "global.h"

#include <vector>
#include <string>
#include <ostream>

#include <ginac/ginac.h>

#include "expr/expression.h"
#include "util/exceptions.h"


#include "its/variablemanager.h"

//typedefs for readability
typedef int TermIndex;
typedef int VariableIndex;
typedef std::vector<Expression> GuardList;
typedef std::map<VariableIndex,Expression> UpdateMap;


/**
 * Represents a rule in an ITRS
 */
struct Rule {
    TermIndex lhsTerm, rhsTerm;
    std::vector<Expression> rhsArgs;
    GuardList guard;
    Expression cost;
};

/**
 * Represents a term (lhs) in an ITRS.
 */
struct Term {
    Term(std::string name) : name(name) {}
    std::string name;
    std::vector<VariableIndex> args;
};

/**
 * Datatype representing an ITRS problem as defined in the sample files
 * @note variable names contain only alphanumeric characters and _
 */
// FIXME: VariableManager inheritance is just to make code compile during refactoring, must be removed!!
class ITRSProblem : public VariableManager {
private:
    ITRSProblem(bool allowDiv, bool checkCosts) : allowDivision(allowDiv), checkCosts(checkCosts) {}

public:
    /**
     * Loads the ITRS data from the given file
     */
    static ITRSProblem loadFromFile(const std::string &filename, bool allowDivision = false, bool checkCosts = true);
    EXCEPTION(FileError,CustomException);

    /**
     * Creates a dummy ITRSProblem that contains just the given rule
     * @note is not very robust and should _only_ be used for testing
     */
    static ITRSProblem dummyITRSforTesting(const std::vector<std::string> vars, const std::vector<std::string> &rules,
                                           bool allowDivision = false, bool checkCosts = true);

    //simple getters
    inline TermIndex getStartTerm() const { return startTerm; }
    inline Term getTerm(TermIndex idx) const { return terms[idx]; }
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
     * Generates a fresh (unused) GiNaC symbol, but does _not_ add it to the itrs variables
     * @return the newly created symbol (_not_ associated with a variable index!)
     */
    ExprSymbol getFreshSymbol(std::string basename) const;

    /**
     * Prints this ITRS contents in a readable but ugly format, for debugging only
     * @param s output stream to print to (e.g. cout)
     */
    void print(std::ostream &s) const;

private:
    //helpers for variable handling
    VariableIndex addVariable(std::string name);
    std::string getFreshName(std::string basename) const;

    //applies replacement map escapeSymbols to the given string (modified by reference)
    void substituteVarnames(std::string &line) const;

    //replaces unbounded variables (not in boundVars) by fresh variables (according to and by modifying unboundedSubs)
    //ex is modified (substitution is applied) and unboundedSubs is extended if new unbound variables are encountered.
    void replaceUnboundedWithFresh(Expression &ex, GiNaC::exmap &unboundedSubs, const ExprSymbolSet &boundVars);

    //used internally by the (not very cleanly written) parser
    void parseRule(std::map<std::string,TermIndex> &knownTerms,
                   std::map<std::string,VariableIndex> &knownVars,
                   const std::string &line);

private:
    /* ITRS Data */
    std::vector<std::string> vars;
    std::set<VariableIndex> freeVars;
    std::vector<Term> terms;
    std::vector<Rule> rules;
    TermIndex startTerm;

    /* settings */
    bool allowDivision;
    bool checkCosts;

    /* for lookup efficiency */
    std::map<std::string,VariableIndex> varMap;

    /* needed since GiNaC::symbols must be referenced later on */
    /* NOTE: symbols with the same name are *NOT* identical to GiNaC */
    std::vector<ExprSymbol> varSymbols;
    ExprList varSymbolList;

    std::map<std::string,std::string> escapeSymbols;
};

#endif // ITRS_H
