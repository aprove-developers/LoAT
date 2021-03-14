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

#include "itsproblem.hpp"
#include "export.hpp"
using namespace std;

std::recursive_mutex ITSProblem::mutex;

ITSProblem::ITSProblem(VariableManager &&varMan) : VariableManager(varMan) {}

bool ITSProblem::isEmpty() const {
    return rules.empty();
}

bool ITSProblem::isLinear() const {
    for (const auto &it : rules) {
        if (!it.second.isLinear()) {
            return false;
        }
    }
    return true;
}

LocationIdx ITSProblem::getInitialLocation() const {
    return initialLocation;
}

bool ITSProblem::isInitialLocation(LocationIdx loc) const {
    return loc == initialLocation;
}

void ITSProblem::setInitialLocation(LocationIdx loc) {
    initialLocation = loc;
}

bool ITSProblem::hasRule(TransIdx transition) const {
    return rules.find(transition) != rules.end();
}

const Rule& ITSProblem::getRule(TransIdx transition) const {
    lock();
    assert(rules.count(transition) > 0);
    const Rule &res = rules.at(transition);
    unlock();
    return res;
}

void ITSProblem::lock() {
    mutex.lock();
}

void ITSProblem::unlock() {
    mutex.unlock();
}

LinearRule ITSProblem::getLinearRule(TransIdx transition) const {
    lock();
    const LinearRule &res = rules.at(transition).toLinear();
    unlock();
    return res;
}

const std::set<LocationIdx>& ITSProblem::getTransitionTargets(TransIdx idx) const {
    return graph.getTransTargets(idx);
}

std::set<TransIdx> ITSProblem::getTransitionsFrom(LocationIdx loc) const {
    return graph.getTransFrom(loc);
}

std::vector<TransIdx> ITSProblem::getTransitionsFromTo(LocationIdx from, LocationIdx to) const {
    return graph.getTransFromTo(from, to);
}

std::set<TransIdx> ITSProblem::getTransitionsTo(LocationIdx loc) const {
    return graph.getTransTo(loc);
}

std::vector<TransIdx> ITSProblem::getAllTransitions() const {
    return graph.getAllTrans();
}

bool ITSProblem::hasTransitionsFrom(LocationIdx loc) const {
    return graph.hasTransFrom(loc);
}

bool ITSProblem::hasTransitionsFromTo(LocationIdx from, LocationIdx to) const {
    return graph.hasTransFromTo(from, to);
}

bool ITSProblem::hasTransitionsTo(LocationIdx loc) const {
    return graph.hasTransTo(loc);
}

std::vector<TransIdx> ITSProblem::getSimpleLoopsAt(LocationIdx loc) const {
    vector<TransIdx> res;
    for (TransIdx rule : getTransitionsFromTo(loc, loc)) {
        if (getRule(rule).isSimpleLoop()) {
            res.push_back(rule);
        }
    }
    return res;
}

std::set<LocationIdx> ITSProblem::getSuccessorLocations(LocationIdx loc) const {
    return graph.getSuccessors(loc);
}

std::set<LocationIdx> ITSProblem::getPredecessorLocations(LocationIdx loc) const {
    return graph.getPredecessors(loc);
}

void ITSProblem::removeRule(TransIdx transition) {
    lock();
    graph.removeTrans(transition);
    rules.erase(transition);
    unlock();
}

TransIdx ITSProblem::addRule(Rule rule) {
    // gather target locations
    set<LocationIdx> rhsLocs;
    for (auto it = rule.rhsBegin(); it != rule.rhsEnd(); ++it) {
        rhsLocs.insert(it->getLoc());
    }

    // add transition and store mapping to rule
    lock();
    TransIdx idx = graph.addTrans(rule.getLhsLoc(), rhsLocs);
    rules.emplace(idx, rule);
    unlock();
    return idx;
}

LocationIdx ITSProblem::addLocation() {
    LocationIdx loc = nextUnusedLocation++;
    locations.insert(loc);
    return loc;
}

LocationIdx ITSProblem::addNamedLocation(std::string name) {
    LocationIdx loc = addLocation();
    locationNames.emplace(loc, name);
    return loc;
}

set<LocationIdx> ITSProblem::getLocations() const {
    return locations;
}

option<string> ITSProblem::getLocationName(LocationIdx idx) const {
    auto it = locationNames.find(idx);
    if (it != locationNames.end()) {
        return it->second;
    }
    return {};
}

string ITSProblem::getPrintableLocationName(LocationIdx idx) const {
    auto it = locationNames.find(idx);
    if (it != locationNames.end()) {
        return it->second;
    }
    return "[" + to_string(idx) + "]";
}


void ITSProblem::removeOnlyLocation(LocationIdx loc) {
    // The initial location must not be removed
    assert(loc != initialLocation);

    locations.erase(loc);
    locationNames.erase(loc);
    set<TransIdx> removed = graph.removeNode(loc);

    // Check that all rules from/to loc were removed before
    assert(removed.empty());
}

std::set<TransIdx> ITSProblem::removeLocationAndRules(LocationIdx loc) {
    // The initial location must not be removed
    assert(loc != initialLocation);

    locations.erase(loc);
    locationNames.erase(loc);
    set<TransIdx> removed = graph.removeNode(loc);

    // Also remove all rules from/to loc
    for (TransIdx t : removed) {
        removeRule(t);
    }
    return removed;
}

void ITSProblem::print(std::ostream &s) const {
    ITSExport::printDebug(*this, s);
}
