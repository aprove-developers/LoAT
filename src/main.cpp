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

#include "its/parser/itsparser.h"
#include "analysis/analysis.h"

#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"

using namespace std;


// Variables for command line flags
string filename;
char benchmarkMode = 'n'; // no benchmark
int timeout = 0; // no timeout
int proofLevel = 2;
bool paperMode = false;
bool printStats = false;
bool printTiming = false;
bool printConfig = false;
bool allowRecursion = true;


void printHelp(char *arg0) {
    cout << "Usage: " << arg0 << " [options] <file>" << endl;
    cout << "Options:" << endl;
    cout << "  --timeout <sec>    Timeout (in seconds), minimum: 10" << endl;
    cout << "  --benchmark <a-f>  Set configuration for the benchmarks in the paper" << endl;
    cout << "  --proof-level <n>  Detail level for proof output (0-3, default 2)" << endl;
    cout << endl;
    cout << "  --plain            Disable colored output" << endl;
    cout << "  --dot <file>       Dump dot output to given file (only for non-recursive problems)" << endl;
    cout << "  --stats            Print some statistics about the performed steps" << endl;
    cout << "  --timing           Print information about time usage" << endl;
    cout << "  --config           Show configuration after handling command line flags" << endl;
    cout << "  --timestamps       Include time stamps in proof output" << endl;
    cout << "  --print-simplified Print simplified program in the input format" << endl;
    cout << endl;
    cout << "  --allow-division   Allow division in the input program (potentially unsound)" << endl;
    cout << "  --no-cost-check    Don't check if costs are nonnegative (potentially unsound)" << endl;
    cout << "  --no-preprocessing Don't try to simplify the program first (which involves SMT)" << endl;
    cout << "  --no-limit-smt     Don't use the SMT encoding for limit problems" << endl;
    cout << "  --paper            Disable heuristics not described in the paper" << endl;
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
        } else if (strcmp("--benchmark",argv[arg]) == 0) {
            benchmarkMode = getNext()[0];
        } else if (strcmp("--proof-level",argv[arg]) == 0) {
            proofLevel = atoi(getNext());
        } else if (strcmp("--plain",argv[arg]) == 0) {
            Config::Output::Colors = false;
            Config::Output::ColorsInITS = false;
        } else if (strcmp("--config",argv[arg]) == 0) {
            printConfig = true;
        } else if (strcmp("--stats",argv[arg]) == 0) {
            printStats = true;
        } else if (strcmp("--timing",argv[arg]) == 0) {
            printTiming = true;
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
        } else if (strcmp("--no-limit-smt",argv[arg]) == 0) {
            Config::Limit::UseSmtEncoding = false;
        } else if (strcmp("--paper",argv[arg]) == 0) {
            paperMode = true;
        } else {
            if (!filename.empty()) {
                cout << "Error: additional argument " << argv[arg] << " (already got filenam: " << filename << ")" << endl;
                exit(1);
            }
            filename = argv[arg];
        }
    }
}


void setBenchmarkConfig(bool conditionalMeter, bool backAccel, bool recursion, bool limitSmt) {
    Config::ForwardAccel::ConditionalMetering = conditionalMeter;
    Config::Accel::UseBackwardAccel = backAccel;
    Config::Limit::UseSmtEncoding = limitSmt;
    allowRecursion = recursion;
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

    // Benchmark and heuristic settings
    switch (benchmarkMode) {
        case 'a': setBenchmarkConfig(false, false, false, false); break;
        case 'b': setBenchmarkConfig(true,  false, false, false); break;
        case 'c': setBenchmarkConfig(false, true,  false, false); break;
        case 'd': setBenchmarkConfig(false, false, true,  false); break;
        case 'e': setBenchmarkConfig(false, false, false, true ); break;
        case 'f': setBenchmarkConfig(true,  true,  true,  true ); break;
        case 'n': break; // benchmark flag not used
        default: cout << "Unknown benchmark setting" << endl; return 1;
    }
    if (paperMode) {
        Config::ForwardAccel::AllowLinearization = false;
        Config::ForwardAccel::ConflictVarHeuristic = false;
        Config::ForwardAccel::ConstantUpdateHeuristic = false;
        Config::Accel::TryNesting = false;
    }

    // Print current configuration (if requested)
    if (printConfig) {
        Config::printConfig(cout, true);
        cout << endl;
    }

    // Timeout
    if (timeout < 0 || (timeout > 0 && timeout < 10)) {
        cout << "Error: timeout must be at least 10 seconds" << endl;
        return 1;
    }
    Timeout::setTimeouts(timeout);
    Timing::start(Timing::Total);

    // Start parsing
    if (filename.empty()) {
        cout << "Error: missing filename" << endl;
        return 1;
    }

    ITSProblem its;
    try {
        its = parser::ITSParser::loadFromFile(filename);
    } catch (const parser::ITSParser::FileError &err) {
        cout << "Error loading file " << filename << ": " << err.what() << endl;
        return 1;
    }

    // Warnings for unsound configurations (they might still be useful for testing or for specific inputs)
    if (Config::Parser::AllowDivision) {
        proofout << endl;
        proofout.setLineStyle(ProofOutput::Warning);
        proofout << "WARNING: Allowing division in the input program can yield unsound results!" << endl;
        proofout.setLineStyle(ProofOutput::Warning);
        proofout << "Division is only sound if the result of a term is always an integer." << endl;
    }
    if (!Config::Analysis::EnsureNonnegativeCosts) {
        proofout << endl;
        proofout.setLineStyle(ProofOutput::Warning);
        proofout << "WARNING: Not checking the costs can yield unsound results!" << endl;
        proofout.setLineStyle(ProofOutput::Warning);
        proofout << "This is only safe if costs in the input program are guaranteed to be nonnegative." << endl;
    }

    // Disable proof output if requested (after issuing the warnings for unsound configuration)
    if (proofLevel == 0) {
        proofout.setEnabled(false);
    }

    // Start the analysis of the parsed ITS problem.
    // Skip ITS problems with nonlinear (i.e., recursive) rules.
    RuntimeResult runtime;
    if (!allowRecursion && !its.isLinear()) {
        proofout.warning("Cannot analyze recursive ITS problem (recursion is disabled).");
    } else {
        runtime = Analysis::analyze(its);
    }
    Timing::done(Timing::Total);

    // Statistics
    if (printStats) {
        cout << endl;
        Stats::print(cout);
    }
    if (printTiming) {
        cout << endl;
        Timing::print(cout);
    }

    // WST style proof output
    proofout.setEnabled(true);
    if (runtime.cpx == Complexity::Nonterm) {
        proofout << endl << "NO" << endl;
    } else {
        proofout << endl << "WORST_CASE(";
        if (runtime.cpx == Complexity::Exp || runtime.cpx == Complexity::NestedExp) proofout << "EXP";
        else if (runtime.cpx == Complexity::Infty) proofout << "INF";
        else if (runtime.cpx == Complexity::Unknown) proofout << "Omega(0)";
        else if (runtime.cpx == Complexity::Const) proofout << "Omega(1)";
        else proofout << "Omega(n^" << runtime.cpx.getPolynomialDegree().toString() << ")";
        proofout << ",?)" << endl;
    }

    return 0;
}
