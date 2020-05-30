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


bool VariableManager::isTempVar(const Var &var) const {
    return temporaryVariables.count(var) > 0;
}

Var VariableManager::addFreshVariable(string basename) {
    return addVariable(getFreshName(basename));
}

Var VariableManager::addFreshTemporaryVariable(string basename) {
    Var x = addVariable(getFreshName(basename));
    temporaryVariables.insert(x);
    return x;
}

Var VariableManager::getFreshUntrackedSymbol(string basename, Expr::Type type) {
    Var res(getFreshName(basename));
    variableNameLookup.emplace(res.get_name(), res);
    untrackedVariables[res] = type;
    return res;
}

Var VariableManager::addVariable(string name) {
    //convert to ginac
    auto sym = Var(name);

    // remember variable
    if (basenameCount.count(name) == 0) {
        basenameCount.emplace(name, 0);
    }
    variables.insert(sym);
    variableNameLookup.emplace(name, sym);

    return sym;
}

string VariableManager::getFreshName(string basename) {
    if (basenameCount.count(basename) == 0) {
        basenameCount.emplace(basename, 0);
        return basename;
    } else {
        uint count = basenameCount.at(basename);
        std::string res = basename + to_string(count);
        while (variableNameLookup.count(res) != 0) {
            ++count;
            res = basename + to_string(count);
        }
        basenameCount[basename] = count + 1;
        return res;
    }
}

const VarSet &VariableManager::getTempVars() const {
    return temporaryVariables;
}

VarSet VariableManager::getVars() const {
    return variables;
}

Expr::Type VariableManager::getType(const Var &x) const {
    if (untrackedVariables.find(x) != untrackedVariables.end()) {
        return untrackedVariables.at(x);
    } else {
        return Expr::Int;
    }
}

BoolExpr VariableManager::freshBoolVar() {
    return buildConst(boolVarCount++);
}
