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

#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>
#include <cctype>

#include <boost/algorithm/string.hpp>

#include "z3toolbox.h"

using namespace std;
using namespace boost::algorithm;

const std::set<char> ITRS::specialCharsInVarNames = {'/', '\'', '.', '_'};


VariableIndex ITRS::addVariable(const std::string &name) {
    assert(variableMap.count(name) == 0);

    // add new variable
    VariableIndex index = variables.size();
    variables.push_back(name);
    variableMap.emplace(name, index);

    // convert to ginac
    ExprSymbol symbol(name);
    ginacSymbols.push_back(symbol);
    varSymbolList.append(symbol);

    return index;
}


VariableIndex ITRS::addFreshVariable(const std::string &basename) {
    return addVariable(getFreshName(basename));
}


std::string ITRS::getFreshName(const std::string &basename) const {
    int num = 0;
    string name = basename;

    while (variableMap.count(name) == 1) {
        name = basename + "_" + to_string(num);
        num++;
    }

    return name;
}


FunctionSymbolIndex ITRS::addFunctionSymbol(const std::string &name) {
    assert(functionSymbolNameMap.count(name) == 0);

    FunctionSymbolIndex index = functionSymbols.size();
    functionSymbols.push_back(name);
    functionSymbolNameMap.emplace(name, index);

    return index;
}


FunctionSymbolIndex ITRS::addFreshFunctionSymbol(const std::string &basename) {
    return addFunctionSymbol(getFreshFunctionSymbolName(basename));
}


std::string ITRS::getFreshFunctionSymbolName(const std::string &basename) const {
    std::string name = basename;

    while (functionSymbolNameMap.count(name) == 1) {
        name += "'";
    }

    return name;
}


void ITRS::print(std::ostream &os) const {
    os << "Variables:";
    for (std::string v : getVariables()) {
        os << " " << v;
    }
    os << std::endl;

    os << "Rules:" << std::endl;
    for (const ITRSRule &r : rules) {
        os << r.lhs;
        os << " -> ";
        os << r.rhs;
        os << " [";
        for (const TT::Expression &ex : r.guard) {
            os << ex << ",";
        }
        os << "], " << r.cost << std::endl;
    }
}


bool ITRS::startFunctionSymbolWasDeclared() const {
    return startFunctionSymbolDeclared;
}


ITRS ITRS::loadFromFile(const string &filename) {
    ITRS res;
    res.load(filename);
    return res;
}


void ITRS::load(const std::string &filename) {
    startFunctionSymbolDeclared = false;
    string startTerm;
    knownVariables.clear();

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
                parseRule(line);
            }
        }
        else {
            if (line[0] != '(') throw FileError("Malformed line: "+line);
            if (line == "(RULES") {
                if (!has_goal || !has_vars || !has_start) {
                    debugParser("WARNING: Missing declarations before RULES-block");
                }
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
                    VariableIndex var = addFreshVariable(escapeVariableName(varname));
                    knownVariables.emplace(varname, var);
                }
                debugParser("Found variable declaration with " << variables.size() << " entries");
                has_vars = true;
            }

            else {
                throw FileError("Unexpected line: "+line);
            }
        }
    }

    //ensure we have at least some rules
    if (rules.empty()) throw FileError("No rules defined");

    //check if startTerm is valid
    if (startTerm.empty()) {
        debugParser("WARNING: Missing start term, defaulting to first function symbol");
        assert(!functionSymbols.empty());
        for (const ITRSRule &rule : rules) {
            std::vector<FunctionSymbolIndex> funcSyms = rule.lhs.getFunctionSymbolsAsVector();
            if (!funcSyms.empty()) {
                startFunctionSymbol = funcSyms.front();
            }
        }

    } else {
        startFunctionSymbolDeclared = true;

        auto it = functionSymbolNameMap.find(startTerm);
        if (it == functionSymbolNameMap.end()) throw FileError("Unknown function symbol: " + startTerm);
        startFunctionSymbol = it->second;
    }

    verifyFunctionSymbolArity();
}


void ITRS::verifyFunctionSymbolArity() {
    class ArityMismatchVisitor : public TT::ConstVisitor {
    public:
        void visitPre(const TT::FunctionSymbol &funSymbol) override {
            FunctionSymbolIndex fsIndex = funSymbol.getFunctionSymbol();
            int fsArity = funSymbol.getArguments().size();

            auto it = arity.find(fsIndex);
            if (it == arity.end()) {
                arity.emplace(fsIndex, fsArity);

            } else {
                if (it->second != fsArity) {
                    throw ArityMismatchException("function symbol redeclared with different arity");
                }
            }
        }

    private:
        std::map<FunctionSymbolIndex,int> arity;
    };

    ArityMismatchVisitor visitor;

    for (const ITRSRule &rule : rules) {
        rule.lhs.traverse(visitor);
        rule.rhs.traverse(visitor);
        rule.cost.traverse(visitor);

        for (const TT::Expression &ex : rule.guard) {
            ex.traverse(visitor);
        }
    }
}


/**
 * Replaces symbols that ginac does not allow by underscores _
 * @param name the variable name to be modified
 */
std::string ITRS::escapeVariableName(const std::string &name) {
    assert(!name.empty());

    std::string escapedName = name;
    for (int i = 0; i < escapedName.length(); ++i) {
        //replace I to avoid interpretation as complex number
        if (escapedName[i] == 'I') {
            escapedName[i] = 'Q';
        }

        //escape any symbol ginac can probably not parse
        if (!isalnum(escapedName[i])) {
            escapedName[i] = '_';
        }
    }

    //ensure name starts with a letter
    if (!isalpha(escapedName[0])) {
        escapedName = "q" + escapedName;
    }

    return escapedName;
}


/**
 * Parses a rule in the ITS file format reading from line.
 * @param res the current state of the ITS that is read (read and modified)
 * @param knownVariables mapping from string to the corresponding variable index (read and modified)
 * @param line the input string
 */
void ITRS::parseRule(const string &line) {
    debugParser("parsing rule: " << line);
    newRule = ITRSRule();
    newRule.cost = Expression(1); //default, if not specified

    /* split string into lhs, rhs (and possibly cost in between) */
    string lhs,rhs,cost;
    string::size_type pos = line.find("-{");
    if (pos != string::npos) {
        //-{ cost }> sytnax
        auto endpos = line.find("}>");
        if (endpos == string::npos) throw ITRS::FileError("Invalid rule, malformed -{ cost }>: "+line);
        cost = line.substr(pos+2,endpos-(pos+2));
        lhs = line.substr(0,pos);
        rhs = line.substr(endpos+2);
    } else {
        //default -> syntax (leave cost string empty)
        pos = line.find("->");
        if (pos == string::npos) throw ITRS::FileError("Invalid rule, -> missing: "+line);
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
            throw ITRS::FileError("Invalid Com_n application, only Com_1 supported");
        }
    }

    parseLeftHandSide(lhs);
    parseRightHandSide(rhs);
    parseCost(cost);
    parseGuard(guard);

    rules.push_back(std::move(newRule));
}

void ITRS::parseLeftHandSide(const std::string &lhs) {
    newRule.lhs = std::move(parseTerm(lhs).ginacify());
}


void ITRS::parseRightHandSide(const std::string &rhs) {
    newRule.rhs = std::move(parseTerm(rhs).ginacify());
}


void ITRS::parseCost(const std::string &cost) {
    if (cost.empty()) {
        newRule.cost = TT::Expression(1);

    } else {
        newRule.cost = std::move(parseTerm(cost).ginacify());
    }
}


void ITRS::parseGuard(const std::string &guard) {
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

            TT::Expression res;
            string::size_type relpos;
            std::string lhs, rhs;
            if ((relpos = term.find("==")) != std::string::npos) {
                lhs = term.substr(0, relpos); trim(lhs);
                rhs = term.substr(relpos + 2); trim(rhs);
                res = parseTerm(lhs) == parseTerm(rhs);

            } else if ((relpos = term.find("!=")) != std::string::npos) {
                lhs = term.substr(0, relpos); trim(lhs);
                rhs = term.substr(relpos + 2); trim(rhs);
                res = parseTerm(lhs) != parseTerm(rhs);

            } else if ((relpos = term.find(">=")) != std::string::npos) {
                lhs = term.substr(0, relpos); trim(lhs);
                rhs = term.substr(relpos + 2); trim(rhs);
                res = parseTerm(lhs) >= parseTerm(rhs);

            } else if ((relpos = term.find("<=")) != std::string::npos) {
                lhs = term.substr(0, relpos); trim(lhs);
                rhs = term.substr(relpos + 2); trim(rhs);
                res = parseTerm(lhs) <= parseTerm(rhs);

            } else if ((relpos = term.find(">")) != std::string::npos) {
                lhs = term.substr(0, relpos); trim(lhs);
                rhs = term.substr(relpos + 1); trim(rhs);
                res = parseTerm(lhs) > parseTerm(rhs);

            } else if ((relpos = term.find("<")) != std::string::npos) {
                lhs = term.substr(0, relpos); trim(lhs);
                rhs = term.substr(relpos + 1); trim(rhs);
                res = parseTerm(lhs) < parseTerm(rhs);

            } else if ((relpos = term.find("=")) != std::string::npos) {
                lhs = term.substr(0, relpos); trim(lhs);
                rhs = term.substr(relpos + 1); trim(rhs);
                res = parseTerm(lhs) == parseTerm(rhs);

            } else {
                throw FileError("Can't parse guard");
            }

            newRule.guard.push_back(std::move(res.ginacify()));

        } while (pos != string::npos);
    }
}


TT::Expression ITRS::parseTerm(const std::string &term) {
    debugTermParser("Parsing " << term);
    toParseReversed = term;
    std::reverse(toParseReversed.begin(), toParseReversed.end());

    nextSymbolCalledOnEmptyInput = false;
    nextSymbol();
    return expression();
}


void ITRS::nextSymbol() {
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
        while (isalnum(toParseReversed.back())
               || specialCharsInVarNames.count(toParseReversed.back()) == 1) {
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

        }  else if (nextChar == '^') {
            symbol = CIRCUMFLEX;

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


bool ITRS::accept(Symbol sym) {
    if (sym == symbol) {
        nextSymbol();
        return true;

    } else {
        return false;
    }
}


bool ITRS::expect(Symbol sym) {
    if (accept(sym)) {
        return true;
    } else {
        throw UnexpectedSymbolException();
    }
}


TT::Expression ITRS::expression() {
    debugTermParser("parsing expression");
    bool negative = false;
    if (symbol == PLUS || symbol == MINUS) {
        negative = (symbol == MINUS);

        nextSymbol();
    }

    TT::Expression result = term();
    if (negative) {
        result = TT::Expression(-1) * result;
    }

    while (symbol == PLUS || symbol == MINUS) {
        negative = (symbol == MINUS);
        nextSymbol();

        TT::Expression temp = term();
        if (negative) {
            result -= temp;

        } else {
            result += temp;
        }
    }

    return result;
}


TT::Expression ITRS::term() {
    debugTermParser("parsing term");
    TT::Expression result = factor();

    while (symbol == TIMES || symbol == SLASH || symbol == CIRCUMFLEX) {
        Symbol op = symbol;
        nextSymbol();

        if (op == TIMES) {
            result *= factor();

        } else if (op == SLASH) {
            throw UnexpectedSymbolException("division is not allowed in the input");
            //result /= factor();

        } else { // CIRCUMFLEX
            result ^= factor();
        }
    }

    return result;
}


TT::Expression ITRS::factor() {
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
        auto it = functionSymbolNameMap.find(name);
        if (it == functionSymbolNameMap.end()) {
            index = functionSymbols.size();
            functionSymbols.emplace_back(name);
            functionSymbolNameMap.emplace(name, index);

        } else {
            index = it->second;
        }

        return TT::Expression(index, name, args);

    } else if (accept(VARIABLE)) {
        std::string name = lastIdent;
        debugTermParser("parsing variable " << name);

        auto it = knownVariables.find(name);
        if (it == knownVariables.end()) {
            debugTermParser("Ooops, " << name << " is a functio symbol of arity 0");

            FunctionSymbolIndex index;
            auto it = functionSymbolNameMap.find(name);
            if (it == functionSymbolNameMap.end()) {
                index = functionSymbols.size();
                functionSymbols.emplace_back(name);
                functionSymbolNameMap.emplace(name, index);

            } else {
                index = it->second;
            }

            std::vector<TT::Expression> args;
            return TT::Expression(index, name, args);
        }

        VariableIndex index = it->second;
        return TT::Expression(getGinacSymbol(index));

    } else if (accept(NUMBER)) {
        debugTermParser("parsing number " << lastIdent);

        GiNaC::numeric num(lastIdent.c_str());
        return TT::Expression(num);

    } else if (accept(LPAREN)) {
        TT::Expression result = expression();
        expect(RPAREN);

        return result;

    } else {
        throw SyntaxErrorException();
    }
}
