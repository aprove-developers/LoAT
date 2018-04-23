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

#include <boost/optional.hpp>

#include "graph/hypergraph.h"
#include "util/exceptions.h"

#include "rule.h"
#include "variablemanager.h"


// Note: LocationData cannot be declared within AbstractITSProblem, since it would then
// be templated over Rule, which makes copying data from ITSProblem to LinearProblem impossible.
namespace ITSProblemInternal {

    // Helper struct to encapsulate members common for ITSProblem and LinearITSProblem
    struct LocationData {
        // the set of all locations (locations are just arbitrary numbers to allow simple addition/deletion)
        std::set<LocationIdx> locations;

        // the initial location
        LocationIdx initialLocation = 0;

        // the next free location index
        LocationIdx nextUnusedLocation = 0;

        // only for output
        std::map<LocationIdx, std::string> locationNames; // only for the original locations, not for added ones
    };
}


template <typename Rule>
class AbstractITSProblem : public VariableManager {
public:
    // Creates an empty ITS problem. The initialLocation is set to 0
    AbstractITSProblem() {}

    // Creates an empty ITS problem with the given variables
    AbstractITSProblem(VariableManager varMan) : VariableManager(varMan) {};

    // True iff there are no rules
    bool isEmpty() const;

    LocationIdx getInitialLocation() const;
    bool isInitialLocation(LocationIdx loc) const;
    void setInitialLocation(LocationIdx loc);

    // query the rule associated with a given transition
    const Rule& getRule(TransIdx transition) const;
    Rule& getRuleMut(TransIdx transition); // Note: the locations of the returned rule must not be changed!

    // query transitions of the graph
    std::vector<TransIdx> getTransitionsFrom(LocationIdx loc) const;
    std::vector<TransIdx> getTransitionsFromTo(LocationIdx from, LocationIdx to) const;
    std::vector<TransIdx> getTransitionsTo(LocationIdx loc) const; // note: rather inefficient

    // helper, combines getTransitionsFrom* + getRule
    std::vector<Rule> getRulesFrom(LocationIdx loc) const; // TODO: unused?
    std::vector<Rule> getRulesFromTo(LocationIdx from, LocationIdx to) const; // TODO: unused?

    // query nodes of the graph
    std::set<LocationIdx> getSuccessorLocations(LocationIdx loc) const;
    std::set<LocationIdx> getPredecessorLocations(LocationIdx loc) const;

    // Mutation of Rules
    void removeRule(TransIdx transition);
    TransIdx addRule(Rule rule);

    // Mutation for Locations
    LocationIdx addLocation();
    LocationIdx addNamedLocation(std::string name);

    // Required for printing (see ITSExport)
    std::set<LocationIdx> getLocations() const;
    boost::optional<std::string> getLocationName(LocationIdx idx) const;

    // Removes a location, but does _not_ care about rules.
    // Rules from/to this location must be removed before calling this!
    void removeOnlyLocation(LocationIdx loc);

    // Removes a location and all rules that visit loc
    void removeLocationAndRules(LocationIdx loc);

    // Print the ITSProblem in a simple, but user-friendly format
    void print(std::ostream &s) const;

private:
    // helper that prints a given rule
    void printRule(const Rule &rule, std::ostream &s) const;

protected:
    // Main structure is the graph, where (hyper-)transitions are annotated with a RuleIdx.
    HyperGraph<LocationIdx> graph;

    // Collection of all rules, identified by the corresponding transitions in the graph.
    // The map allows to efficiently add/delete rules.
    std::map<TransIdx, Rule> rules;

    // All remaining members
    ITSProblemInternal::LocationData data;
};


class LinearITSProblem : public AbstractITSProblem<LinearRule> {
public:
    LocationIdx getTransitionTarget(TransIdx) const;

private:
    // Creates an empty problem with the given variables, required for toLinearProblem()
    LinearITSProblem(VariableManager varMan) : AbstractITSProblem(varMan) {}

    // For the conversion in toLinearProblem()
    friend class ITSProblem;
};


class ITSProblem : public AbstractITSProblem<NonlinearRule> {
public:
    const std::vector<LocationIdx>& getTransitionTargets(TransIdx) const;

    bool isLinear() const;
    LinearITSProblem toLinearProblem() const;
};


// Since the implementation of AbstractITSProblem is not in the header,
// we have to explicitly declare all required instantiations.
template class AbstractITSProblem<LinearRule>;
template class AbstractITSProblem<NonlinearRule>;


#endif // ITSPROBLEM_H
