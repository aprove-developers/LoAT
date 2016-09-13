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

#include "itrsproblem.h"

#include <iostream>
#include <map>

ITRSProblem ITRSProblem::loadFromFile(const std::string &filename) {
    ITRSProblem res;
    res.load(filename);
    res.processRules();
    return res;
}

void ITRSProblem::processRules() {
    // Initialize the function symbols
    for (const std::string &name : getFunctionSymbolNames()) {
        // Add a new FunctionSymbol object for every function symbol
        functionSymbols.emplace_back(name);
    }

    // process the rules
    for (const ITRSRule &rule : getITRSRules()) {
        Rule res;

        if (!rule.lhs.isSimple()) {
            debugParser("lhs " << rule.lhs << " is not simple");
            throw UnsupportedITRSException("lhs is not simple");
        }

        // get the function symbol index of the lhs
        res.lhs = rule.lhs.getFunctionSymbolsAsVector().front();

        // get the arguments of the lhs
        std::vector<TT::Expression> arguments = rule.lhs.getUpdates();

        // make sure they are either variables or numbers
        std::vector<VariableIndex> argumentVariables;
        for (const TT::Expression &arg : arguments) {
            if (arg.info(TT::InfoFlag::Variable)) {
                ExprSymbol symbol = GiNaC::ex_to<GiNaC::symbol>(arg.toGiNaC());
                argumentVariables.push_back(getVariableIndex(symbol.get_name()));

            } else if (arg.info(TT::InfoFlag::Number)) {
                debugParser("moving condition to guard: " << arg);
                VariableIndex index = addFreshVariable("x");
                res.guard.push_back(TT::Expression(getGinacSymbol(index)) == arg);
                argumentVariables.push_back(index);

            } else {
                throw UnsupportedITRSException("lhs contains arithmetic expressions");
            }
        }

        GiNaC::exmap symbolSub;
        FunctionSymbol &funSymbol = functionSymbols[res.lhs];
        if (!funSymbol.isDefined()) {
            funSymbol.setDefined();
            funSymbol.getArguments() = std::move(argumentVariables);

        } else {
            // Check if variables differ from previous occurences and provide substitution if necessary
            const std::vector<VariableIndex> &previousVars = funSymbol.getArguments();
            assert(previousVars.size() == argumentVariables.size());

            for (int i = 0; i < argumentVariables.size(); ++i) {
                VariableIndex vOld = previousVars[i];
                VariableIndex vNew = argumentVariables[i];
                if (vOld != vNew) {
                    symbolSub[getGinacSymbol(vNew)] = getGinacSymbol(vOld);
                }
            }

            if (!symbolSub.empty()) {
                debugParser("ITRS Warning: funapp redeclared with different arguments: " << funSymbol.getName());
            }
        }

        // Apply symbolSub to expression that were added
        // while moving conditions from the lhs to the guard
        for (TT::Expression &ex : res.guard) {
            ex = ex.substitute(symbolSub);
        }

        //collect the lhs variables that are used
        ExprSymbolSet boundSymbols;
        for (VariableIndex vi : funSymbol.getArguments()) {
            boundSymbols.insert(getGinacSymbol(vi));
        }

        // process the rhs
        GiNaC::exmap freeVarSub;
        res.rhs = rule.rhs.substitute(symbolSub);
        replaceUnboundedWithFresh(res.rhs.getVariables(), boundSymbols, freeVarSub);
        res.rhs = res.rhs.substitute(freeVarSub);

        // process the cost
        if (!rule.cost.is_polynomial(getGinacVarList())) {
            throw UnsupportedITRSException("non-polynomial cost");
        }
        res.cost = rule.cost.subs(symbolSub);
        replaceUnboundedWithFresh(res.cost.getVariables(), boundSymbols, freeVarSub);
        res.cost = res.cost.subs(freeVarSub);

        // make sure that user-given costs are always positive
        if (!(GiNaC::is_a<GiNaC::numeric>(res.cost)
              && GiNaC::ex_to<GiNaC::numeric>(res.cost).is_positive())) {
            res.guard.push_back(TT::Expression(res.cost > 0).ginacify());
        }

        // process the guard
        for (const TT::Expression &ex : rule.guard) {
            if (!ex.hasNoFunctionSymbols()) {
                throw UnsupportedITRSException("guard contains function symbols");
            }

            res.guard.push_back(ex.substitute(symbolSub));
            replaceUnboundedWithFresh(res.guard.back().getVariables(), boundSymbols, freeVarSub);
            res.guard.back() = res.guard.back().substitute(freeVarSub);
        }

        // add the rule
        rules.push_back(std::move(res));
    }
}


//setup substitution for unbound variables (i.e. not on lhs) by new fresh variables
void ITRSProblem::replaceUnboundedWithFresh(const ExprSymbolSet &checkSymbols,
                                            ExprSymbolSet &boundedVars,
                                            GiNaC::exmap &addToSub) {
    for (const ExprSymbol &sym : checkSymbols) {
        if (boundedVars.count(sym) == 0) {
            VariableIndex vFree = addFreshFreeVariable("free");
            ExprSymbol freeSym = getGinacSymbol(vFree);

            addToSub.emplace(sym, freeSym);
             // don't substitute other occurences of this variable by a different variable
            boundedVars.insert(sym);
            boundedVars.insert(freeSym);
        }
    }
}


bool ITRSProblem::isFreeVariable(const ExprSymbol &var) const {
    for (VariableIndex i : freeVariables) {
        if (var == getGinacSymbol(i)) {
            return true;
        }
    }

    return false;
}


VariableIndex ITRSProblem::addFreshFreeVariable(const std::string &basename) {
    VariableIndex var = addFreshVariable(basename);
    freeVariables.insert(var);
    return var;
}


VariableIndex ITRSProblem::addChainingVariable() {
    VariableIndex vi = addFreshFreeVariable("z");
    chainingVariables.insert(vi);
    return vi;
}

bool ITRSProblem::isChainingVariable(VariableIndex index) const {
    return chainingVariables.count(index) > 0;
}


bool ITRSProblem::isChainingVariable(const ExprSymbol &var) const {
    for (VariableIndex i : chainingVariables) {
        if (var == getGinacSymbol(i)) {
            return true;
        }
    }

    return false;
}


ExprSymbol ITRSProblem::getFreshSymbol(const std::string &basename) const {
    return ExprSymbol(getFreshName(basename));
}


FunctionSymbolIndex ITRSProblem::addFunctionSymbolVariant(FunctionSymbolIndex fs) {
    assert(fs >= 0);
    assert(fs < functionSymbols.size());
    const FunctionSymbol &oldFun = functionSymbols[fs];

    FunctionSymbolIndex newFunIndex = addFreshFunctionSymbol(oldFun.getName());

    FunctionSymbol newFun(getFunctionSymbolName(newFunIndex));
    if (oldFun.isDefined()) {
        newFun.setDefined();
    }
    newFun.getArguments() = oldFun.getArguments();
    functionSymbols.push_back(std::move(newFun));

    return newFunIndex;
}


void ITRSProblem::print(std::ostream &os) const {
    os << "Variables:";
    for (std::string v : getVariables()) {
        os << " ";
        if (isFreeVariable(getVariableIndex(v)))
            os << "_" << v << "_";
        else
            os << v;
    }
    os << std::endl;

    os << "Rules:" << std::endl;
    for (const Rule &r : rules) {
        printLhs(r.lhs, os);
        os << " -> ";
        os << r.rhs;
        os << " [";
        for (const TT::Expression &ex : r.guard) {
            os << ex << ",";
        }
        os << "], " << r.cost << std::endl;
    }
}


void ITRSProblem::printLhs(FunctionSymbolIndex fun, std::ostream &os) const {
    const FunctionSymbol &funSymbol = functionSymbols[fun];
    os << funSymbol.getName() << "(";

    auto &funcVars = funSymbol.getArguments();
    for (int i = 0; i < funcVars.size(); ++i) {
        if (i > 0) {
            os << ", ";
        }

        os << getVariableName(funcVars[i]);
    }
    os << ")";
}
