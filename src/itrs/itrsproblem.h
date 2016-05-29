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

#ifndef ITRSPROBLEM_H
#define ITRSPROBLEM_H

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <ginac/ginac.h>

#include "itrs.h"
#include "global.h"
#include "exceptions.h"
#include "expression.h"
#include "term.h"

typedef std::vector<Expression> GuardList;
typedef std::map<VariableIndex,Expression> UpdateMap;

/**
 * Represents a rule in an ITS
 */
struct Rule {
    FunctionSymbolIndex lhs;
    TT::Expression rhs;
    TT::ExpressionVector guard;
    TT::Expression cost;
};


class FunctionSymbol {
public:
    FunctionSymbol(std::string name)
        : name(name), defined(false) {
    }

    std::string getName() const {
        return name;
    }

    bool isDefined() const {
        return defined;
    }

    void setDefined() {
        defined = true;
    }

    std::vector<VariableIndex>& getArguments() {
        return arguments;
    }

    const std::vector<VariableIndex>& getArguments() const {
        return arguments;
    }

private:
    std::string name;
    bool defined;
    std::vector<VariableIndex> arguments;
};

/**
 * Datatype representing an ITS problem as defined in the sample files
 * @note variable names contain only alphanumeric characters and _
 */

class ITRSProblem : public ITRS {
public:
    /**
     * Loads the ITS data from the given file
     */
    static ITRSProblem loadFromFile(const std::string &filename);

    inline const FunctionSymbol& getFunctionSymbol(FunctionSymbolIndex index) const { return functionSymbols[index]; }

    inline const std::vector<Rule>& getRules() const { return rules; }

    inline const std::set<VariableIndex>& getFreeVariables() const { return freeVariables; }
    inline bool isFreeVariable(VariableIndex index) const { return freeVariables.count(index) > 0; }
    bool isFreeVariable(const ExprSymbol &var) const;

    /**
     * Adds a fresh free variable based on the given name
     * @return the VariableIndex of the newly added variable
     */
    VariableIndex addFreshFreeVariable(const std::string &basename);

    /**
     * Generates a fresh (unused) GiNaC symbol, but does _not_ add it to the ITS variables
     * @return the newly created symbol (_not_ associated with a variable index!)
     */
    ExprSymbol getFreshSymbol(const std::string &basename) const;

    FunctionSymbolIndex addFunctionSymbolVariant(FunctionSymbolIndex fs);

    /**
     * Prints this ITS contents in a readable but ugly format, for debugging only
     * @param s output stream to print to (e.g. cout)
     */
    void print(std::ostream &os) const override;

    void printLhs(FunctionSymbolIndex fun, std::ostream &os) const;

private:
    ITRSProblem() {}
    void processRules();
    EXCEPTION(UnsupportedITRSException, CustomException);

    void replaceUnboundedWithFresh(const ExprSymbolSet &checkSymbols,
                                   ExprSymbolSet &boundedVars,
                                   GiNaC::exmap &addToSub);

private:
    std::vector<Rule> rules;

    std::vector<FunctionSymbol> functionSymbols;

    std::set<VariableIndex> freeVariables;
};

#endif // ITRSPROBLEM_H