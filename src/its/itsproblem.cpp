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

#include "itsproblem.h"
#include "export.h"

//using namespace ITSProblemInternal;
using namespace std;
using boost::optional;


template<typename Rule>
bool AbstractITSProblem<Rule>::isEmpty() const {
    return rules.empty();
}

template<typename Rule>
LocationIdx AbstractITSProblem<Rule>::getInitialLocation() const {
    return data.initialLocation;
}

template<typename Rule>
bool AbstractITSProblem<Rule>::isInitialLocation(LocationIdx loc) const {
    return loc == data.initialLocation;
}

template<typename Rule>
void AbstractITSProblem<Rule>::setInitialLocation(LocationIdx loc) {
    data.initialLocation = loc;
}

template<typename Rule>
const Rule &AbstractITSProblem<Rule>::getRule(TransIdx transition) const {
    return rules.at(transition);
}

template<typename Rule>
std::vector<TransIdx> AbstractITSProblem<Rule>::getTransitionsFrom(LocationIdx loc) const {
    return graph.getTransFrom(loc);
}

template<typename Rule>
std::vector<TransIdx> AbstractITSProblem<Rule>::getTransitionsFromTo(LocationIdx from, LocationIdx to) const {
    return graph.getTransFromTo(from, to);
}

template<typename Rule>
std::vector<TransIdx> AbstractITSProblem<Rule>::getTransitionsTo(LocationIdx loc) const {
    return graph.getTransTo(loc);
}

template<typename Rule>
std::vector<Rule> AbstractITSProblem<Rule>::getRulesFrom(LocationIdx loc) const {
    vector<Rule> res;
    for (TransIdx trans : graph.getTransFrom(loc)) {
        res.push_back(getRule(trans));
    }
    return res;
}

template<typename Rule>
std::vector<Rule> AbstractITSProblem<Rule>::getRulesFromTo(LocationIdx from, LocationIdx to) const {
    vector<Rule> res;
    for (TransIdx trans : graph.getTransFromTo(from, to)) {
        res.push_back(getRule(trans));
    }
    return res;
}

template<typename Rule>
std::set<LocationIdx> AbstractITSProblem<Rule>::getSuccessorLocations(LocationIdx loc) const {
    return graph.getSuccessors(loc);
}

template<typename Rule>
std::set<LocationIdx> AbstractITSProblem<Rule>::getPredecessorLocations(LocationIdx loc) const {
    return graph.getPredecessors(loc);
}

template<typename Rule>
void AbstractITSProblem<Rule>::removeRule(TransIdx transition) {
    graph.removeTrans(transition);
}

template<typename Rule>
TransIdx AbstractITSProblem<Rule>::addRule(Rule rule) {
    // gather target locations
    vector<LocationIdx> rhsLocs;
    for (auto it = rule.rhsBegin(); it != rule.rhsEnd(); ++it) {
        rhsLocs.push_back(it->loc);
    }

    // add transition and store mapping to rule
    TransIdx idx = graph.addTrans(rule.getLhsLoc(), rhsLocs);
    rules.emplace(idx, rule);
    return idx;
}

template<typename Rule>
LocationIdx AbstractITSProblem<Rule>::addLocation() {
    LocationIdx loc = data.nextUnusedLocation++;
    data.locations.insert(loc);
    return loc;
}

template<typename Rule>
LocationIdx AbstractITSProblem<Rule>::addNamedLocation(std::string name) {
    LocationIdx loc = addLocation();
    data.locationNames.emplace(loc, name);
    return loc;
}

template<typename Rule>
set<LocationIdx> AbstractITSProblem<Rule>::getLocations() const {
    return data.locations;
}

template<typename Rule>
optional<string> AbstractITSProblem<Rule>::getLocationName(LocationIdx idx) const {
    auto it = data.locationNames.find(idx);
    if (it != data.locationNames.end()) {
        return it->second;
    }
    return {};
}

template<typename Rule>
void AbstractITSProblem<Rule>::removeOnlyLocation(LocationIdx loc) {
    // The initial location must not be removed
    assert(loc != data.initialLocation);

    data.locations.erase(loc);
    data.locationNames.erase(loc);
    set<TransIdx> removed = graph.removeNode(loc);

    // Check that all rules from/to loc were removed before
    assert(removed.empty());
}

template<typename Rule>
void AbstractITSProblem<Rule>::removeLocationAndRules(LocationIdx loc) {
    // The initial location must not be removed
    assert(loc != data.initialLocation);

    data.locations.erase(loc);
    data.locationNames.erase(loc);
    set<TransIdx> removed = graph.removeNode(loc);

    // Also remove all rules from/to loc
    for (TransIdx t : removed) {
        removeRule(t);
    }
}

template<typename Rule>
void AbstractITSProblem<Rule>::print(std::ostream &s) const {
    ITSExport<Rule>::printDebug(*this, s);
}


// ###################
// ## Linear        ##
// ###################

LocationIdx LinearITSProblem::getTransitionTarget(TransIdx idx) const {
    assert(graph.getTransTargets(idx).size() == 1);
    return graph.getTransTargets(idx).front();
}


// ###################
// ## Nonlinear     ##
// ###################

const std::vector<LocationIdx> &ITSProblem::getTransitionTargets(TransIdx idx) const {
    return graph.getTransTargets(idx);
}

bool ITSProblem::isLinear() const {
    for (const auto &it : rules) {
        if (!it.second.isLinear()) {
            return false;
        }
    }
    return true;
}

LinearITSProblem ITSProblem::toLinearProblem() const {
    assert(isLinear());

    LinearITSProblem res;
    res.data = data;
    res.graph = graph;

    for (const auto &it : rules) {
        res.rules.emplace(it.first, it.second.toLinearRule());
    }
    return res;
}
