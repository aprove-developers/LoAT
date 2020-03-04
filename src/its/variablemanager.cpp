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

#include "variablemanager.hpp"
#include "itsproblem.hpp"

using namespace std;


bool VariableManager::hasVarIdx(VariableIdx idx) const {
    return idx < variables.size();
}

string VariableManager::getVarName(VariableIdx idx) const {
    return variables[idx].name;
}

VariableIdx VariableManager::getVarIdx(const ExprSymbol &var) const {
    return variableNameLookup.at(var.get_name());
}

const set<VariableIdx> &VariableManager::getTempVars() const {
    return temporaryVariables;
}

bool VariableManager::isTempVar(VariableIdx idx) const {
    return temporaryVariables.count(idx) > 0;
}

bool VariableManager::isTempVar(const ExprSymbol &var) const {
    VariableIdx idx = getVarIdx(var);
    return temporaryVariables.count(idx) > 0;
}

ExprSymbol VariableManager::getVarSymbol(VariableIdx idx) const {
    return variables[idx].symbol;
}

VariableIdx VariableManager::addFreshVariable(string basename) {
    return addVariable(getFreshName(basename));
}

VariableIdx VariableManager::addFreshTemporaryVariable(string basename) {
    VariableIdx idx = addVariable(getFreshName(basename));
    temporaryVariables.insert(idx);
    return idx;
}

ExprSymbol VariableManager::getFreshUntrackedSymbol(string basename, Expression::Type type) {
    const ExprSymbol &res = ExprSymbol(getFreshName(basename));
    untrackedVariables[res] = type;
    return res;
}

VariableIdx VariableManager::addVariable(string name) {
    // find new index
    VariableIdx idx = variables.size();

    //convert to ginac
    auto sym = ExprSymbol(name);

    // remember variable
    variables.push_back({name, sym});
    variableNameLookup.emplace(name, idx);

    return idx;
}

string VariableManager::getFreshName(string basename) const {
    int n = 1;
    string name = basename;

    while (variableNameLookup.count(name) != 0) {
        name = basename + "_" + to_string(n);
        n++;
    }

    return name;
}

size_t VariableManager::getVariableCount() const {
    return variables.size();
}

Expression::Type VariableManager::getType(const ExprSymbol &x) const {
    if (variableNameLookup.find(x.get_name()) == variableNameLookup.end()) {
        return untrackedVariables.at(x);
    } else {
        return Expression::Int;
    }
}
