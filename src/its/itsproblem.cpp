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

//using namespace ITSProblemInternal;
using namespace std;


template<typename Rule>
LocationIdx AbstractITSProblem<Rule>::getInitialLocation() const {
    return 0;
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
    TransIdx idx = graph.addTrans(rule.lhs.loc, rhsLocs);
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
bool AbstractITSProblem<Rule>::hasVarIdx(VariableIdx idx) const {
    return idx < data.variables.size();
}

template<typename Rule>
std::string AbstractITSProblem<Rule>::getVarName(VariableIdx idx) const {
    return data.variables[idx].name;
}

template<typename Rule>
VariableIdx AbstractITSProblem<Rule>::getVarIdx(std::string name) const {
    return data.variableNameLookup.at(name);
}

template<typename Rule>
VariableIdx AbstractITSProblem<Rule>::getVarIdx(const ExprSymbol &var) const {
    return data.variableNameLookup.at(var.get_name());
}

template<typename Rule>
const std::set<VariableIdx> &AbstractITSProblem<Rule>::getTempVars() const {
    return data.temporaryVariables;
}

template<typename Rule>
bool AbstractITSProblem<Rule>::isTempVar(VariableIdx idx) const {
    return data.temporaryVariables.count(idx) > 0;
}

template<typename Rule>
bool AbstractITSProblem<Rule>::isTempVar(const ExprSymbol &var) const {
    VariableIdx idx = getVarIdx(var.get_name());
    return data.temporaryVariables.count(idx);
}

template<typename Rule>
ExprSymbol AbstractITSProblem<Rule>::getGinacSymbol(VariableIdx idx) const {
    return data.variables[idx].symbol;
}

template<typename Rule>
ExprList AbstractITSProblem<Rule>::getGinacVarList() const {
    return data.variableSymbolList;
}

template<typename Rule>
VariableIdx AbstractITSProblem<Rule>::addFreshVariable(std::string basename) {
    return addVariable(getFreshName(basename));
}

template<typename Rule>
VariableIdx AbstractITSProblem<Rule>::addFreshTemporaryVariable(std::string basename) {
    VariableIdx idx = addVariable(getFreshName(basename));
    data.temporaryVariables.insert(idx);
    return idx;
}

template<typename Rule>
ExprSymbol AbstractITSProblem<Rule>::getFreshUntrackedSymbol(std::string basename) const {
    return ExprSymbol(getFreshName(basename));
}

template<typename Rule>
VariableIdx AbstractITSProblem<Rule>::addVariable(std::string name) {
    // find new index
    VariableIdx idx = data.variables.size();

    //convert to ginac
    auto sym = GiNaC::symbol(name);
    data.variableSymbolList.append(sym);

    // remember variable
    data.variables.push_back({name, sym});
    data.variableNameLookup.emplace(name, idx);

    return idx;
}

template<typename Rule>
std::string AbstractITSProblem<Rule>::getFreshName(std::string basename) const {
    int n = 1;
    string name = basename;

    while (data.variableNameLookup.count(name) != 0) {
        name = basename + "_" + to_string(n);
        n++;
    }

    return name;
}

template<typename Rule>
void AbstractITSProblem<Rule>::print(std::ostream &s) const {
    s << "Variables:";
    for (VariableIdx i=0; i < data.variables.size(); ++i) {
        s << " " << data.variables[i].name;
        if (isTempVar(i)) {
            s << "*";
        }
    }
    s << endl;

    s << "Nodes:";
    for (LocationIdx loc : data.locations) {
        s << " " << loc;
        auto it = data.locationNames.find(loc);
        if (it != data.locationNames.end()) {
            s << "/" << it->second;
        }
        if (loc == data.initialLocation) {
            s << "*";
        }
    }
    s << endl;

    s << "Transitions:" << endl;
    for (LocationIdx loc : data.locations) {
        for (TransIdx trans : getTransitionsFrom(loc)) {
            s << "    ";
            s << setw(3) << trans << ": ";
            printRule(getRule(trans), s);
        }
    }
}

template<typename Rule>
void AbstractITSProblem<Rule>::printRule(const Rule &rule, std::ostream &s) const {
    auto printLoc = [&](LocationIdx loc) {
        auto it = data.locationNames.find(loc);
        if (it != data.locationNames.end()) {
            s << it->second;
        } else {
            s << "[" << loc << "]";
        }
    };

    printLoc(rule.lhs.loc);
    s << " -> ";

    for (auto it = rule.rhsBegin(); it != rule.rhsEnd(); ++it) {
        printLoc(it->loc);
        s << " : ";

        for (auto upit : it->update) {
            s << getVarName(upit.first) << "'";
            s << "=" << upit.second;
            s << ", ";
        }
    }

    if (rule.lhs.guard.empty()) {
        s << "[]";
    } else {
        s << "[ ";
        for (int i=0; i < rule.lhs.guard.size(); ++i) {
            if (i > 0) s << " && ";
            s << rule.lhs.guard[i];
        }
        s << " ]";
    }
    s << ", cost: " << rule.lhs.cost;
    s << endl;
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
