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

#include <iostream>
#include <boost/algorithm/string.hpp>
#include "analysis/analysis.hpp"
#include "its/parser/itsparser.hpp"
#include "its/sexpressionparser/parser.hpp"
#include "its/t2parser/t2parser.hpp"

#include "util/timeout.hpp"

using namespace std;

// Variables for command line flags
string filename;
int timeout = 0; // no timeout
int proofLevel = 2;
bool printConfig = false;

void printHelp(char *arg0) {
    cout << "Usage: " << arg0 << " [options] <file>" << endl;
    cout << "Options:" << endl;
    cout << "  --timeout <sec>                                  Timeout (in seconds), minimum: 10" << endl;
    cout << "  --proof-level <n>                                Detail level for proof output (0-3, default 2)" << endl;
    cout << endl;
    cout << "  --plain                                          Disable colored output" << endl;
    cout << "  --dot <file>                                     Dump dot output to given file (only for non-recursive problems)" << endl;
    cout << "  --config                                         Show configuration after handling command line flags" << endl;
    cout << "  --timestamps                                     Include time stamps in proof output" << endl;
    cout << "  --print-simplified                               Print simplified program in the input format" << endl;
    cout << endl;
    cout << "  --allow-division                                 Allow division in the input program (potentially unsound)" << endl;
    cout << "  --no-cost-check                                  Don't check if costs are nonnegative (potentially unsound)" << endl;
    cout << "  --no-preprocessing                               Don't try to simplify the program first (which involves SMT)" << endl;
    cout << "  --limit-strategy <smt|calculus|smtAndCalculus>   strategy for limit problems" << endl;
    cout << "  --no-const-cpx                                   Don't check for constant complexity (might improve performance)" << endl;
    cout << "  --nonterm                                        Just try to prove non-termination" << endl;
}


void parseFlags(int argc, char *argv[]) {
    int arg=0;

    auto getNext = [&]() {
        if (arg < argc-1) {
            return argv[++arg];
        } else {
            cout << "Error: Argument missing for " << argv[arg] << endl;
            exit(1);
        }
    };

    while (++arg < argc) {
        if (strcmp("--help",argv[arg]) == 0) {
            printHelp(argv[0]);
            exit(1);
        }
        else if (strcmp("--dot",argv[arg]) == 0) {
            Config::Output::DotFile = getNext();
        } else if (strcmp("--timeout",argv[arg]) == 0) {
            timeout = atoi(getNext());
        } else if (strcmp("--proof-level",argv[arg]) == 0) {
            proofLevel = atoi(getNext());
        } else if (strcmp("--plain",argv[arg]) == 0) {
            Config::Output::Colors = false;
            Config::Output::ColorsInITS = false;
        } else if (strcmp("--config",argv[arg]) == 0) {
            printConfig = true;
        } else if (strcmp("--timestamps",argv[arg]) == 0) {
            Config::Output::Timestamps = true;
        } else if (strcmp("--print-simplified",argv[arg]) == 0) {
            Config::Output::ExportSimplified = true;
        } else if (strcmp("--allow-division",argv[arg]) == 0) {
            Config::Parser::AllowDivision = true;
        } else if (strcmp("--no-preprocessing",argv[arg]) == 0) {
            Config::Analysis::Preprocessing = false;
        } else if (strcmp("--no-cost-check",argv[arg]) == 0) {
            Config::Analysis::EnsureNonnegativeCosts = false;
        } else if (strcmp("--limit-strategy",argv[arg]) == 0) {
            const std::string &strategy = getNext();
            bool found = false;
            for (Config::Limit::PolynomialLimitProblemStrategy* s: Config::Limit::PolyStrategies) {
                if (boost::iequals(strategy, s->name())) {
                    Config::Limit::PolyStrategy = s;
                    found = true;
                    break;
                }
            }
            if (!found) {
                cout << "Unknown strategy " << strategy << " for limit problems, defaulting to " << Config::Limit::PolyStrategy->name();
            }
        } else if (strcmp("--no-const-cpx",argv[arg]) == 0) {
            Config::Analysis::ConstantCpxCheck = false;
        } else if (strcmp("--nonterm",argv[arg]) == 0) {
            Config::Analysis::NonTermMode = true;
            Config::Analysis::ConstantCpxCheck = false;
        } else {
            if (!filename.empty()) {
                cout << "Error: additional argument " << argv[arg] << " (already got filenam: " << filename << ")" << endl;
                exit(1);
            }
            filename = argv[arg];
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }

    // Parse and interpret command line flags
    parseFlags(argc, argv);

    // Proof output
    Config::Output::ProofAccel = (proofLevel >= 1);
    Config::Output::ProofLimit = (proofLevel >= 2);
    Config::Output::ProofChain = (proofLevel >= 3);

    // Print current configuration (if requested)
    if (printConfig) {
        Config::printConfig(cout, true);
        cout << endl;
    }

    // Timeout
    if (timeout < 0 || (timeout > 0 && timeout < 10)) {
        cerr << "Error: timeout must be at least 10 seconds" << endl;
        return 1;
    }
    Timeout::setTimeouts(timeout);

    // Start parsing
    if (filename.empty()) {
        cerr << "Error: missing filename" << endl;
        return 1;
    }

    ITSProblem its;
    try {
        if (boost::algorithm::ends_with(filename, ".koat")) {
            its = parser::ITSParser::loadFromFile(filename);
        } else if (boost::algorithm::ends_with(filename, ".smt2")) {
            its = sexpressionparser::Parser::loadFromFile(filename);
        } else if (boost::algorithm::ends_with(filename, ".t2")) {
            its = t2parser::T2Parser::loadFromFile(filename);
        }
    } catch (const parser::ITSParser::FileError &err) {
        cout << "Error loading file " << filename << ": " << err.what() << endl;
        return 1;
    }

    // Warnings for unsound configurations (they might still be useful for testing or for specific inputs)
    if (Config::Parser::AllowDivision) {
        std::cerr << "WARNING: Allowing division in the input program can yield unsound results!" << std::endl;
        std::cerr << "Division is only sound if the result of a term is always an integer." << std::endl;
    }
    if (!Config::Analysis::EnsureNonnegativeCosts) {
        std::cerr << "WARNING: Not checking the costs can yield unsound results!" << std::endl;
        std::cerr << "This is only safe if costs in the input program are guaranteed to be nonnegative." << std::endl;
    }

    // Disable proof output if requested
    if (proofLevel == 0) {
        proofout.setEnabled(false);
    }

    // Start the analysis of the parsed ITS problem.
    // Skip ITS problems with nonlinear (i.e., recursive) rules.
    RuntimeResult runtime = Analysis::analyze(its);

    // WST style proof output
    cout << runtime.cpx.toWstString() << std::endl;

    proofout.print();

    return 0;
}
