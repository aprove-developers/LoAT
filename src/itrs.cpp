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

bool isNonVariableChar(char c) {
    return c == '+' || c == '-' || c == '*' || c == '^' || c == '/' //operators
        || c == '>' || c == '<' || c == '=' //relations
        || c == ' ' || c == '&' || c == ':' || c == ',' //separators
        || c == '(' || c == ')' || c == '[' || c == ']'; //brackets
}

bool isValidVarname(const std::string &name) {
    for (char c : name) {
        if (isNonVariableChar(c)) {
            return false;
        }
    }
    return true;
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
               || (nextpos < line.length() && !isNonVariableChar(line[nextpos]))) {
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


void ITRSProblem::replaceUnboundedWithFresh(Expression &ex, GiNaC::exmap &unboundedSubs, const ExprSymbolSet &boundVars) {
    for (const ExprSymbol &sym : ex.getVariables()) {
        if (boundVars.count(sym) == 0 && unboundedSubs.count(sym) == 0) {
            VariableIndex vFree = addFreshVariable("free");
            this->freeVars.insert(vFree);
            ExprSymbol freeSym = getGinacSymbol(vFree);
            unboundedSubs[sym] = freeSym;
        }
    }
    ex = ex.subs(unboundedSubs);
}

/**
 * Parses a rule in the ITRS file format reading from line.
 * @param res the current state of the ITRS that is read (read and modified)
 * @param knownTerms mapping from string to the corresponding term index (read and modified)
 * @param knownVars mapping from string to the corresponding variable index (read and modified)
 * @param line the input string
 */
void ITRSProblem::parseRule(map<string,TermIndex> &knownTerms, map<string,VariableIndex> &knownVars, const string &line) {
    debugParser("parsing rule: " << line);
    Rule rule;
    rule.cost = Expression(1); //default, if not specified

    auto getTermIndex = [&](const string &s) -> TermIndex {
        if (knownTerms.find(s) == knownTerms.end()) {
            knownTerms[s] = this->terms.size();
            this->terms.push_back(Term(s));
        }
        return knownTerms[s];
    };

    GiNaC::exmap unboundedSubs; //replace unbounded by fresh variables
    GiNaC::exmap unifyArgSubs; //unify arguments across rules
    ExprSymbolSet boundSymbols;

    /* split string into lhs, rhs (and possibly cost in between) */
    string lhs,rhs,cost;
    string::size_type pos = line.find("-{");
    if (pos != string::npos) {
        //-{ cost }> sytnax
        auto endpos = line.find("}>");
        auto midpos = line.find(",",pos);
        if (endpos == string::npos || midpos == string::npos || midpos >= endpos) {
            throw ITRSProblem::FileError("Invalid rule, malformed -{ lowerbound, upperbound }>: "+line);
        }
        cost = line.substr(pos+2,midpos-(pos+2));
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


    /* lhs */
    string fun;
    vector<string> args;
    parseFunapp(lhs,fun,args);
    //parse variables
    vector<VariableIndex> argVars;
    for (string &v : args) {
        substituteVarnames(v);
        auto it = knownVars.find(v);
        if (it == knownVars.end()) throw ITRSProblem::FileError("Unknown variable in lhs: " + v);
        argVars.push_back(it->second);
        boundSymbols.insert(getGinacSymbol(it->second));
    }
    //check if variable names differ from previous occurences and provide substitution if necessary
    rule.lhsTerm = getTermIndex(fun);
    if (this->terms[rule.lhsTerm].args.empty()) {
        this->terms[rule.lhsTerm].args = std::move(argVars);
    } else {
        if (this->terms[rule.lhsTerm].args.size() != argVars.size()) {
            throw ITRSProblem::FileError("Funapp redeclared with different argument count: "+fun);
        }
        for (int i=0; i < argVars.size(); ++i) {
            VariableIndex vOld = this->terms[rule.lhsTerm].args[i];
            VariableIndex vNew = argVars[i];
            if (vOld != vNew) {
                unifyArgSubs[getGinacSymbol(vNew)] = getGinacSymbol(vOld); //must be applied after renaming fresh variables
            }
        }
        if (!unifyArgSubs.empty()) debugParser("ITRS Warning: funapp redeclared with different arguments: " << fun);
    }

    /* rhs */
    args.clear();
    ExprSymbolSet rhsSymbols;
    parseFunapp(rhs,fun,args);
    rule.rhsTerm = getTermIndex(fun);
    for (string &v : args) {
        substituteVarnames(v);
        if (!allowDivision && v.find('/') != string::npos) throw ITRSProblem::FileError("Divison is not allowed in the input");
        Expression argterm = Expression::fromString(v,this->varSymbolList);
        replaceUnboundedWithFresh(argterm, unboundedSubs, boundSymbols);
        rule.rhsArgs.push_back(argterm.subs(unifyArgSubs));
    }

    /* cost */
    if (!cost.empty()) {
        substituteVarnames(cost);
        if (!allowDivision && cost.find('/') != string::npos) throw ITRSProblem::FileError("Divison is not allowed in the input");
        rule.cost = Expression::fromString(cost,this->varSymbolList);
        if (!rule.cost.is_polynomial(this->varSymbolList)) throw ITRSProblem::FileError("Non polynomial cost in the input");
        replaceUnboundedWithFresh(rule.cost, unboundedSubs, boundSymbols);
        rule.cost = rule.cost.subs(unifyArgSubs);
    }

    /* guard */
    ExprSymbolSet guardSymbols;
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
            Expression guardTerm = Expression::fromString(term,this->varSymbolList);
            replaceUnboundedWithFresh(guardTerm, unboundedSubs, boundSymbols);
            rule.guard.push_back(guardTerm.subs(unifyArgSubs));
        } while (pos != string::npos);
    }
    //ensure user given costs are positive
    if (!cost.empty()) {
        rule.guard.push_back(rule.cost >= 0);
    }

    this->rules.push_back(rule);
}


ITRSProblem ITRSProblem::loadFromFile(const string &filename, bool allowDivision) {
    ITRSProblem res(allowDivision);
    string startTerm;
    map<string,TermIndex> knownTerms;
    map<string,VariableIndex> knownVars;

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
                res.parseRule(knownTerms,knownVars,line);
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
                    if (!isValidVarname(varname)) throw FileError("Invalid variable name: "+varname);
                    string escapedname = varname;
                    escapeVarname(escapedname);
                    VariableIndex vi = res.addFreshVariable(escapedname);
                    escapedname = res.getVarname(vi);
                    knownVars[escapedname] = vi;
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

    //if a term appeared only on rhs, the argument list is empty, add some dummy arguments (vars from 0 on)
    for (const Rule &r : res.rules) {
        if (res.terms[r.rhsTerm].args.empty()) {
            for (VariableIndex v=0; v < r.rhsArgs.size(); ++v) {
                res.terms[r.rhsTerm].args.push_back(v);
            }
        }
    }

    //check if startTerm is valid
    if (startTerm.empty()) {
        debugParser("WARNING: Missing start term, defaulting to first rule lhs");
        assert(!res.rules.empty());
        res.startTerm = res.rules[0].lhsTerm;
    } else {
        auto it = knownTerms.find(startTerm);
        if (it == knownTerms.end()) throw FileError("No rules for start term: " + startTerm);
        res.startTerm = it->second;
    }

    return res;
}



ITRSProblem ITRSProblem::dummyITRSforTesting(const vector<string> vars, const vector<string> &rules, bool allowDivision) {
    ITRSProblem res(allowDivision);
    map<string,TermIndex> knownTerms;

    //create vars
    map<string,VariableIndex> knownVars;
    for (const string &name : vars) {
        assert(res.varMap.count(name) == 0);
        knownVars[name] = res.addVariable(name);
    }

    //parse all rules
    for (const string &rule : rules) {
        res.parseRule(knownTerms,knownVars,rule);
    }

    //if a term appeared only on rhs, the argument list is empty, add some dummy arguments (vars from 0 on)
    for (const Rule &r : res.rules) {
        if (res.terms[r.rhsTerm].args.empty()) {
            for (VariableIndex v=0; v < r.rhsArgs.size(); ++v) {
                res.terms[r.rhsTerm].args.push_back(v);
            }
        }
    }

    //make the first appearing term the start term
    res.startTerm = 0;
    return res;
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
    for (Rule r : rules) {
        s << terms[r.lhsTerm].name << "(";
        for (VariableIndex v : terms[r.lhsTerm].args) s << vars[v] << ",";
        s << ") -> ";
        s << terms[r.rhsTerm].name << "(";
        for (Expression e : r.rhsArgs) {
            printExpr(e);
            s << ",";
        }
        s << ") [";
        for (Expression e : r.guard) {
            printExpr(e);
            s << ",";
        }
        s << "]" << endl;
    }
}

