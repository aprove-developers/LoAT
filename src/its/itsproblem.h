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

#ifndef ITSPROBLEM_H
#define ITSPROBLEM_H

#include "global.h"

#include <set>
#include <vector>
#include <string>
#include <ostream>

#include "graph/hypergraph.h"
#include "exceptions.h"
#include "rule.h"


// Note: Data cannot be declared within AbstractITSProblem, since it would then
// be templated over Rule, which makes copying data from ITSProblem to LinearProblem impossible.
namespace ITSProblemInternal {

    // Data stored for each variable
    struct Variable {
        std::string name;
        ExprSymbol symbol;
    };

    // Helper struct to encapsulate members common for ITSProblem and LinearITSProblem
    struct Data {
        // List of all variables (VariableIdx is an index in this list; a Variable is a name and a ginac symbol)
        // Note: Variables are never removed, so this list is appended, but otherwise not modified
        std::vector<Variable> variables;

        // the set of variables (identified by their index) that are used as temporary variables (on rule rhss)
        std::set<VariableIdx> temporaryVariables;

        // the set of all locations (locations are just arbitrary numbers to allow simple addition/deletion)
        std::set<LocationIdx> locations;

        // the initial location
        LocationIdx initialLocation = 0;

        // the next free location index
        LocationIdx nextUnusedLocation = 0;

        // only for efficiency
        std::map<std::string,VariableIdx> variableNameLookup; // reverse mapping for variables
        ExprList variableSymbolList; // list of all variable symbols, since this is needed by ginac (what for?)

        // only for output
        std::map<LocationIdx, std::string> locationNames; // only for the original locations, not for added ones
    };
}


template <typename Rule>
class AbstractITSProblem {
public:
    // Creates an empty ITS problem. The initialLocation is set to 0
    AbstractITSProblem() {}

    LocationIdx getInitialLocation() const;
    void setInitialLocation(LocationIdx loc);

    // query the rule associated with a given transition
    const Rule& getRule(TransIdx transition) const;

    // query transitions of the graph
    std::vector<TransIdx> getTransitionsFrom(LocationIdx loc) const;
    std::vector<TransIdx> getTransitionsFromTo(LocationIdx from, LocationIdx to) const;

    // helper, combines getTransitionsFrom* + getRule
    std::vector<Rule> getRulesFrom(LocationIdx loc) const;
    std::vector<Rule> getRulesFromTo(LocationIdx from, LocationIdx to) const;

    // Mutation of Rules
    void removeRule(TransIdx transition);
    TransIdx addRule(Rule rule);

    // Mutation for Locations
    LocationIdx addLocation();
    LocationIdx addNamedLocation(std::string name);

    // Removes a location, but does _not_ care about rules.
    // Rules from/to this location must be removed before calling this!
    void removeOnlyLocation(LocationIdx loc);

    // Removes a location and all rules that visit loc
    void removeLocationAndRules(LocationIdx loc);

    // Some simple getters
    bool hasVarIdx(VariableIdx idx) const;
    std::string getVarName(VariableIdx idx) const;
    VariableIdx getVarIdx(std::string name) const;
    VariableIdx getVarIdx(const ExprSymbol &var) const;

    const std::set<VariableIdx>& getTempVars() const;
    bool isTempVar(VariableIdx idx) const;
    bool isTempVar(const ExprSymbol &var) const;

    ExprSymbol getGinacSymbol(VariableIdx idx) const;
    ExprList getGinacVarList() const;

    /**
     * Adds a new fresh variable based on the given name
     * (the given name is used if it is still available, otherwise it is modified)
     * @return the VariableIdx of the newly added variable
     */
    VariableIdx addFreshVariable(std::string basename);
    VariableIdx addFreshTemporaryVariable(std::string basename);

    /**
     * Generates a fresh (unused) GiNaC symbol, but does _not_ add it to the list of variables
     * @warning the name of the created symbol is not stored, so it may be re-used by future calls!
     * @return the newly created symbol (_not_ associated with a variable index!)
     */
    ExprSymbol getFreshUntrackedSymbol(std::string basename) const;

    /**
     * Prints this ITSProblem in a readable but ugly format, for debugging only
     * @param s output stream to print to (e.g. cout)
     */
    void print(std::ostream &s) const;

private:
    // helpers for variable handling
    VariableIdx addVariable(std::string name);
    std::string getFreshName(std::string basename) const;

    // helper that adds a transition for the given rule to the graph
    virtual TransIdx addTransitionForRule(const Rule &rule) = 0;

    // helper that prints a given rule, called from print
    virtual void printRule(const Rule &rule, std::ostream &s) const = 0;

protected:
    // Main structure is the graph, where (hyper-)transitions are annotated with a RuleIdx.
    HyperGraph<LocationIdx> graph;

    // Collection of all rules, identified by the corresponding transitions in the graph.
    // The map allows to efficiently add/delete rules.
    std::map<TransIdx, Rule> rules;

    // All remaining members
    ITSProblemInternal::Data data;
};


class LinearITSProblem : public AbstractITSProblem<LinearRule> {
public:
    LocationIdx getTransitionTarget(TransIdx) const;

private:
    TransIdx addTransitionForRule(const LinearRule &rule) override;
    void printRule(const LinearRule &rule, std::ostream &s) const override;

    friend class ITSProblem;
};


class ITSProblem : public AbstractITSProblem<NonlinearRule> {
public:
    const std::vector<LocationIdx>& getTransitionTargets(TransIdx) const;

    bool isLinear() const;
    LinearITSProblem toLinearProblem() const;

private:
    TransIdx addTransitionForRule(const NonlinearRule &rule) override;
    void printRule(const NonlinearRule &rule, std::ostream &s) const override;
};


// Since the implementation of AbstractITSProblem is not in the header,
// we have to explicitly declare all required instantiations.
template class AbstractITSProblem<LinearRule>;
template class AbstractITSProblem<NonlinearRule>;


#endif // ITSPROBLEM_H
