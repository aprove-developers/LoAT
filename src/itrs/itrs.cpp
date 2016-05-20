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

#include "itrs.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>
#include <cctype>

#include <boost/algorithm/string.hpp>

#include "z3toolbox.h"

using namespace std;
using namespace boost::algorithm;

/**
 * Replaces symbols that ginac does not allow by underscores _
 * @param name the variable name to be modified
 */
static void escapeVarname(string &name) {
    assert(!name.empty());
    for (int i=0; i < name.length(); ++i) {
        if (name[i] == 'I') name[i] = 'Q'; //replace I to avoid interpretation as complex number
        if (!isalnum(name[i])) name[i] = '_'; //escape any symbol ginac can probably not parse
    }
    if (!isalpha(name[0])) {
        name = "q" + name; //ensure name starts with a letter
    }
}


void ITRSProblem::substituteVarnames(string &line) const {
    set<size_t> replacedPositions;
    for (auto it : escapeSymbols) {
        size_t pos = 0;
        while ((pos = line.find(it.first,pos)) != string::npos) {
            //ensure no character is replaced more than once, and also
            //ensure a complete variable is substituted, i.e. the name does not continue to the left and/or right
            size_t nextpos = pos+it.first.length();
            if (replacedPositions.count(pos) > 0
               || (pos > 0 && (line[pos-1] == '_' || isalnum(line[pos-1])))
               || (nextpos < line.length() && (line[nextpos] == '_' || isalnum(line[nextpos])))) {
                pos++;
                continue;
            }
            //otherwise it can be replaced
            line.replace(pos, it.first.size(), it.second);
            for (int i=0; i < it.second.length(); ++i) replacedPositions.insert(pos+i);
            pos += it.second.length();
        }
    }
}


ITRSProblem ITRSProblem::loadFromFile(const string &filename) {
    ITRSProblem res;
    string startTerm;
    res.knownVars.clear();

    ifstream file(filename);
    if (!file.is_open())
        throw FileError("Unable to open file: "+filename);

    bool has_vars, has_goal, has_start;
    has_vars = has_goal = has_start = false;

    bool in_rules = false;

    string line;
    while (getline(file, line)) {
        trim(line);
        if (line.length() == 0) continue;
        if (line[0] == '#') continue; //allow line comments with #
        if (in_rules) {
            if (line == ")")
                in_rules = false;
            else {
                res.parseRule(line);
            }
        }
        else {
            if (line[0] != '(') throw FileError("Malformed line: "+line);
            if (line == "(RULES") {
                if (!has_goal || !has_vars || !has_start) throw FileError("Missing declarations before RULES-block");
                in_rules = true;
            }
            else if (line.rfind(')') != line.length()-1)
                throw FileError("Malformed line (missing closing paren): "+line);

            else if (line == "(GOAL COMPLEXITY)")
                has_goal = true;

            else if (line.substr(1,9) == "STARTTERM") {
                if (has_start) throw FileError("Multiple STARTTERM declarations");

                if (line.find("CONSTRUCTOR-BASED") != string::npos) {
                    //support invalid format for benchmark, assume first rule defines start symbol
                    startTerm.clear();
                } else {
                    string keyword = "FUNCTIONSYMBOLS ";
                    auto pos = line.find(keyword);
                    if (pos == string::npos) throw FileError("Invalid start term declaration: "+line);
                    pos += keyword.length();

                    auto endpos = line.find(')',pos);
                    if (endpos == string::npos) throw FileError("Missing ) in term declaration: "+line);
                    startTerm = line.substr(pos,endpos-pos);
                }
                debugParser("Found start term: " << startTerm);
                has_start = true;
            }

            else if (line.substr(1,3) == "VAR") {
                if (has_vars) throw FileError("Multiple VAR declarations");
                stringstream ss(line.substr(4,line.length()-1-4));
                string varname;
                while (ss >> varname) {
                    string escapedname = varname;
                    escapeVarname(escapedname);
                    VariableIndex vi = res.addFreshVariable(escapedname);
                    escapedname = res.getVarname(vi);
                    res.knownVars[escapedname] = vi;
                    if (escapedname != varname) {
                        res.escapeSymbols[varname] = escapedname;
                    }
                }
                debugParser("Found variable declaration with " << res.vars.size() << " entries");
                has_vars = true;
            }

            else {
                throw FileError("Unexpected line: "+line);
            }
        }
    }

    //ensure we have at least some rules
    if (res.rules.empty()) throw FileError("No rules defined");

    //check if startTerm is valid
    if (startTerm.empty()) {
        debugParser("WARNING: Missing start term, defaulting to first rule lhs");
        assert(!res.rules.empty());
        res.startTerm = res.rules[0].lhs;
    } else {
        auto it = res.functionSymbolMap.find(startTerm);
        if (it == res.functionSymbolMap.end()) throw FileError("No rules for start term: " + startTerm);
        res.startTerm = it->second;
    }

    return res;
}

/**
 * Splits a function application (line) in function name (fun) and argument strings (args)
 * e.g. f(x,y) will result in "f" and {"x","y"}
 */
static void parseFunapp(const string &line, string &fun, vector<string> &args) {
    args.clear();
    string::size_type pos = line.find('(');
    if (pos == string::npos) throw ITRSProblem::FileError("Invalid funapp (missing open paren): " + line);
    if (line.rfind(')') != line.length()-1) throw ITRSProblem::FileError("Invalid funapp (bad close paren): "+line);

    fun = line.substr(0,pos);
    string::size_type startpos = pos+1;
    while ((pos = line.find(',',startpos)) != string::npos) {
        string arg = line.substr(startpos,pos-startpos);
        trim(arg);
        if (arg.empty()) throw ITRSProblem::FileError("Empty argument in funapp: "+line);
        args.push_back(arg);
        startpos = pos+1;
    }
    string lastarg = line.substr(startpos,line.length()-1-startpos);
    trim(lastarg);
    if (!lastarg.empty()) {
        args.push_back(lastarg);
    } else if (!args.empty()) {
        throw ITRSProblem::FileError("Empty last argument in funapp: "+line);
    }
}


/**
 * Parses a rule in the ITS file format reading from line.
 * @param res the current state of the ITS that is read (read and modified)
 * @param knownVars mapping from string to the corresponding variable index (read and modified)
 * @param line the input string
 */
void ITRSProblem::parseRule(const string &line) {
    debugParser("parsing rule: " << line);
    newRule = ITRSRule();
    newRule.cost = Expression(1); //default, if not specified

    symbolSubs.clear();
    boundSymbols.clear();

    /* split string into lhs, rhs (and possibly cost in between) */
    string lhs,rhs,cost;
    string::size_type pos = line.find("-{");
    if (pos != string::npos) {
        //-{ cost }> sytnax
        auto endpos = line.find("}>");
        if (endpos == string::npos) throw ITRSProblem::FileError("Invalid rule, malformed -{ cost }>: "+line);
        cost = line.substr(pos+2,endpos-(pos+2));
        lhs = line.substr(0,pos);
        rhs = line.substr(endpos+2);
    } else {
        //default -> syntax (leave cost string empty)
        pos = line.find("->");
        if (pos == string::npos) throw ITRSProblem::FileError("Invalid rule, -> missing: "+line);
        lhs = line.substr(0,pos);
        rhs = line.substr(pos+2);
    }
    trim(lhs);

    /* split rhs into rhs funapp and guard */
    string guard;
    if ((pos = rhs.find("[")) != string::npos) {
        guard = rhs.substr(pos+1,rhs.length()-1-(pos+1));
        trim(guard);
        rhs = rhs.substr(0,pos);
    } else if ((pos = rhs.find(":|:")) != string::npos) {
        guard = rhs.substr(pos+3);
        trim(guard);
        rhs = rhs.substr(0,pos);
    }
    trim(rhs);

    if (rhs.substr(0,4) == "Com_") {
        if (rhs[4] == '1' && rhs[5] == '(' && rhs[rhs.size()-1] == ')') {
            rhs = rhs.substr(6,rhs.length()-6-1);
            trim(rhs);
        } else {
            throw ITRSProblem::FileError("Invalid Com_n application, only Com_1 supported");
        }
    }

    parseLhs(lhs);
    parseRhs(rhs);
    parseCost(cost);
    parseGuard(guard);

    this->rules.push_back(newRule);
}

void ITRSProblem::parseLhs(std::string &lhs) {
    string fun;
    vector<string> args;
    parseFunapp(lhs, fun, args);
    //parse variables
    vector<VariableIndex> argVars;
    for (string &v : args) {
        substituteVarnames(v);

        if (v.find('/') != string::npos) throw ITRSProblem::FileError("Divison is not allowed in the input");
        Expression argterm = Expression::fromString(v, this->varSymbolList);

        if (GiNaC::is_a<GiNaC::symbol>(argterm)) {
            GiNaC::symbol sym = GiNaC::ex_to<GiNaC::symbol>(argterm);

            auto it = knownVars.find(sym.get_name());
            if (it == knownVars.end()) {
                throw ITRSProblem::FileError("Unknown variable in lhs: " + v);
            }
            argVars.push_back(it->second);

        } else if (GiNaC::is_a<GiNaC::numeric>(argterm)) {
            debugParser("moving condition to guard: " << v);
            VariableIndex index = addFreshVariable("x", true);

            newRule.guard.push_back(getGinacSymbol(index) == argterm);
            argVars.push_back(index);

        } else {
            throw ITRSProblem::FileError("Unsupported expression on lhs: " + v);
        }
    }

    auto funMapIt = functionSymbolMap.find(fun);
    // Add function symbol if it is not already present
    if (funMapIt == functionSymbolMap.end()) {
        newRule.lhs = functionSymbols.size();
        functionSymbols.emplace_back(fun);
        functionSymbolMap.emplace(fun, newRule.lhs);
        debugParser(fun << " is " << newRule.lhs);

    } else {
        newRule.lhs = funMapIt->second;
        debugParser(fun << " is " << newRule.lhs << " (already present)");
    }

    // Check if variable names differ from previous occurences and provide substitution if necessary
    FunctionSymbol &funSymbol = functionSymbols[newRule.lhs];
    if (!funSymbol.isDefined()) {
        funSymbol.setDefined();
        funSymbol.getArguments() = std::move(argVars);

    } else {
        std::vector<VariableIndex> &previousVars = funSymbol.getArguments();

        if (previousVars.size() != argVars.size()) {
            throw ITRSProblem::FileError("Funapp redeclared with different argument count: " + fun);
        }

        for (int i = 0; i < argVars.size(); ++i) {
            VariableIndex vOld = previousVars[i];
            VariableIndex vNew = argVars[i];
            if (vOld != vNew) {
                symbolSubs[getGinacSymbol(vNew)] = getGinacSymbol(vOld);
            }
        }

        if (!symbolSubs.empty()) {
            debugParser("ITS Warning: funapp redeclared with different arguments: " << fun);
        }
        //collect the lhs variables that are used (i.e. the ones of the previous occurence)
        for (VariableIndex vi : previousVars) {
            boundSymbols.insert(getGinacSymbol(vi));
        }
    }

    // Apply symbolSubs to expression that were added
    // while moving conditions from the lhs to the guard
    for (Expression &expression : newRule.guard) {
        expression = expression.subs(symbolSubs);
    }

    //collect the lhs variables that are bound (i.e. the ones of the previous occurence)
    for (VariableIndex vi : funSymbol.getArguments()) {
        boundSymbols.insert(getGinacSymbol(vi));
    }
}


void ITRSProblem::parseRhs(std::string &rhs) {
    newRule.rhs = parseTerm(rhs).ginacify().substitute(symbolSubs);

    ExprSymbolSet rhsSymbols;
    for (const ExprSymbol &sym : newRule.rhs.getVariables()) {
        rhsSymbols.insert(sym);
    }

    //replace unbound variables (not on lhs) by new fresh variables to ensure correctness
    if (replaceUnboundedWithFresh(rhsSymbols)) {
        newRule.rhs = newRule.rhs.substitute(symbolSubs);

    }
}


void ITRSProblem::parseCost(std::string &cost) {
    ExprSymbolSet costSymbols;

    if (!cost.empty()) {
        substituteVarnames(cost);
        if (cost.find('/') != string::npos) throw ITRSProblem::FileError("Divison is not allowed in the input");
        newRule.cost = Expression::fromString(cost, this->varSymbolList).subs(symbolSubs);
        if (!newRule.cost.is_polynomial(this->varSymbolList)) throw ITRSProblem::FileError("Non polynomial cost in the input");
        newRule.cost.collectVariables(costSymbols);
    }

    //replace unbound variables (not on lhs) by new fresh variables to ensure correctness
    if (replaceUnboundedWithFresh(costSymbols)) {
        newRule.cost = newRule.cost.subs(symbolSubs);
    }

    //ensure user given costs are positive
    if (!cost.empty()) {
        newRule.guard.push_back(newRule.cost > 0);
    }
}


void ITRSProblem::parseGuard(std::string &guard) {
    ExprSymbolSet guardSymbols;

    string::size_type pos;
    if (!guard.empty()) {
        string::size_type startpos = 0;
        do {
            pos = min(guard.find("/\\",startpos),guard.find("&&",startpos));
            string term = guard.substr(startpos,(pos == string::npos) ? string::npos : (pos-startpos));
            trim(term);
            startpos = pos+2;
            //ignore TRUE in guards (used to indicate an empty guard in some files)
            if (term == "TRUE" || term.empty()) continue;
            substituteVarnames(term);
            if (term.find('/') != string::npos) throw ITRSProblem::FileError("Divison is not allowed in the input");
            Expression guardTerm = Expression::fromString(term,this->varSymbolList).subs(symbolSubs);
            guardTerm.collectVariables(guardSymbols);
            newRule.guard.push_back(guardTerm);
        } while (pos != string::npos);
    }

    //replace unbound variables (not on lhs) by new fresh variables to ensure correctness
    if (replaceUnboundedWithFresh(guardSymbols)) {
        debugParser("ITS Note: free variables in guard: " << guard);
        for (Expression &guardExpr : newRule.guard) {
            guardExpr = guardExpr.subs(symbolSubs);
        }
    }
}


//setup substitution for unbound variables (i.e. not on lhs) by new fresh variables
bool ITRSProblem::replaceUnboundedWithFresh(const ExprSymbolSet &checkSymbols) {
    bool added = false;
    for (const ExprSymbol &sym : checkSymbols) {
        if (boundSymbols.count(sym) == 0) {
            VariableIndex vFree = addFreshVariable("free");
            this->freeVars.insert(vFree);
            ExprSymbol freeSym = getGinacSymbol(vFree);
            symbolSubs[sym] = freeSym;
            boundSymbols.insert(freeSym);
            added = true;
        }
    }
    return added;
}


VariableIndex ITRSProblem::addVariable(string name) {
    //add new variable
    VariableIndex vi = vars.size();
    varMap[name] = vi;
    vars.push_back(name);

    //convert to ginac
    auto sym = GiNaC::symbol(name);
    varSymbols.push_back(sym);
    varSymbolList.append(sym);

    return vi;
}


string ITRSProblem::getFreshName(string basename) const {
    int num = 1;
    string name = basename;
    while (varMap.find(name) != varMap.end()) {
        name = basename + "_" + to_string(num);
        num++;
    }
    return name;
}


bool ITRSProblem::isFreeVar(const ExprSymbol &var) const {
    for (VariableIndex i : freeVars) {
        if (var == getGinacSymbol(i)) {
            return true;
        }
    }

    return false;
}


VariableIndex ITRSProblem::addFreshVariable(string basename, bool free) {
    VariableIndex v = addVariable(getFreshName(basename));
    if (free) freeVars.insert(v);
    return v;
}


ExprSymbol ITRSProblem::getFreshSymbol(string basename) const {
    return ExprSymbol(getFreshName(basename));
}


FunctionSymbolIndex ITRSProblem::addFunctionSymbolVariant(FunctionSymbolIndex fs) {
    assert(fs >= 0);
    assert(fs < functionSymbols.size());
    const FunctionSymbol &oldFun = functionSymbols[fs];
    std::string newName = oldFun.getName();
    while (functionSymbolMap.count(newName) == 1) {
        newName += "'";
    }

    FunctionSymbol newFun(newName);
    if (oldFun.isDefined()) {
        newFun.setDefined();
    }
    newFun.getArguments() = oldFun.getArguments();

    FunctionSymbolIndex newFunIndex = functionSymbols.size();
    functionSymbols.push_back(std::move(newFun));
    functionSymbolMap.emplace(newName, newFunIndex);

    return newFunIndex;
}


void ITRSProblem::print(ostream &s) const {
    auto printExpr = [&](const Expression &e) {
        s << e;
    };

    s << "Variables:";
    for (string v : vars) {
        s << " ";
        if (isFreeVar(getVarindex(v)))
            s << "_" << v << "_";
        else
            s << v;
    }
    s << endl;

    s << "Rules:" << endl;
    for (ITRSRule r : rules) {
        printLhs(r.lhs, s);
        s << " -> ";
        s << r.rhs;
        s << " [";
        for (Expression e : r.guard) {
            s << e << ",";
        }
        s << "], " << r.cost << endl;
    }
}


TT::Expression ITRSProblem::parseTerm(const std::string &term) {
    debugTermParser("Parsing " << term);
    toParseReversed = term;
    std::reverse(toParseReversed.begin(), toParseReversed.end());

    nextSymbolCalledOnEmptyInput = false;
    nextSymbol();
    return expression();
}


void ITRSProblem::nextSymbol() {
    trim_right(toParseReversed);

    if (nextSymbolCalledOnEmptyInput) {
        throw UnexpectedEndOfTextException();
    }

    if (toParseReversed.empty()) {
        nextSymbolCalledOnEmptyInput = true;
        return;
    }

    char nextChar = toParseReversed.back();
    debugTermParser("read symbol: " << nextChar);

    if (isdigit(nextChar)) {
        lastIdent.clear();
        while (isdigit(toParseReversed.back())) {
            lastIdent += toParseReversed.back();
            toParseReversed.pop_back();
        }

        symbol = NUMBER;

    } else if (isalpha(nextChar)) {
        lastIdent.clear();
        while (isalnum(toParseReversed.back())) {
            lastIdent += toParseReversed.back();
            toParseReversed.pop_back();
        }

        if (toParseReversed.back() == '(') {
            symbol = FUNCTIONSYMBOL;
        } else {
            symbol = VARIABLE;
        }

    } else {
        if (nextChar == '+') {
            symbol = PLUS;

        } else if (nextChar == '-') {
            symbol = MINUS;

        }  else if (nextChar == '*') {
            symbol = TIMES;

        }  else if (nextChar == '/') {
            symbol = SLASH;

        } else if (nextChar == '(') {
            symbol = LPAREN;

        } else if (nextChar == ')') {
            symbol = RPAREN;

        } else if (nextChar == ',') {
            symbol = COMMA;

        } else {
            throw UnknownSymbolException("Unknown symbol: " + nextChar);
        }

        toParseReversed.pop_back();
    }
}


bool ITRSProblem::accept(Symbol sym) {
    if (sym == symbol) {
        nextSymbol();
        return true;

    } else {
        return false;
    }
}


bool ITRSProblem::expect(Symbol sym) {
    if (accept(sym)) {
        return true;
    } else {
        throw UnexpectedSymbolException();
    }
}


TT::Expression ITRSProblem::expression() {
    debugTermParser("parsing expression");
    bool negative = false;
    if (symbol == PLUS || symbol == MINUS) {
        negative = (symbol == MINUS);

        nextSymbol();
    }

    TT::Expression result = term();
    if (negative) {
        result = TT::Expression(*this, GiNaC::numeric(-1)) * result;
    }

    while (symbol == PLUS || symbol == MINUS) {
        negative = (symbol == MINUS);
        nextSymbol();

        TT::Expression temp = term();
        if (negative) {
            result = result - temp;

        } else {
            result = result + temp;
        }
    }

    return result;
}


TT::Expression ITRSProblem::term() {
    debugTermParser("parsing term");
    TT::Expression result = factor();

    while (symbol == TIMES || symbol == SLASH) {
        nextSymbol();
        result = result * factor();
    }

    return result;
}


TT::Expression ITRSProblem::factor() {
    debugTermParser("parsing factor");
    if (accept(FUNCTIONSYMBOL)) {
        std::string name = lastIdent;
        debugTermParser("parsing function symbol " << name);

        expect(LPAREN);

        std::vector<TT::Expression> args;
        do {
            args.push_back(expression());
        } while (accept(COMMA));

        expect(RPAREN);

        FunctionSymbolIndex index;
        auto it = functionSymbolMap.find(name);
        if (it == functionSymbolMap.end()) {
            index = functionSymbols.size();
            functionSymbols.emplace_back(name);
            functionSymbolMap.emplace(name, index);

        } else {
            index = it->second;
        }

        return TT::Expression(*this, index, args);

    } else if (accept(VARIABLE)) {
        std::string name = lastIdent;
        substituteVarnames(name);
        debugTermParser("parsing variable " << name);

        auto it = knownVars.find(name);
        if (it == knownVars.end()) {
            throw UnknownVariableException(name);
        }
        VariableIndex index = it->second;

        return TT::Expression(*this, getGinacSymbol(index));

    } else if (accept(NUMBER)) {
        debugTermParser("parsing number " << lastIdent);

        GiNaC::numeric num(lastIdent.c_str());
        return TT::Expression(*this, num);

    } else if (accept(LPAREN)) {
        TT::Expression result = expression();
        expect(RPAREN);

        return result;

    } else {
        throw SyntaxErrorException();
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

        os << getVarname(funcVars[i]);
    }
    os << ")";
}
