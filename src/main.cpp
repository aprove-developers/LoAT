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

#include <ginac/ginac.h>
#include <purrs.hh>
#include <z3++.h>

#include "util/exceptions.h"

#include <sstream>
#include <fstream>

using namespace std;


#include "preprocess.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"


#include "parser/itsparser.h"
#include "strategy/linear.h"
#include "strategy_nonlinear/nl_linear.h"


/**
 * Print the compile flags chosen in global.h
 */
void printConfig() {
    cout << "Compiled with configuration:" << endl;

    cout << " Contract check SAT:                 ";
#ifdef CONTRACT_CHECK_SAT
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif

    cout << " Contract approximate SAT:           ";
#ifdef CONTRACT_CHECK_SAT_APPROXIMATE
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif

    cout << " Z3 treat power as mult up to:       " << Z3_MAX_EXPONENT << endl;

    cout << " Simplify before every loop ranking: ";
#ifdef SELFLOOPS_ALWAYS_SIMPLIFY
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif

    cout << " Instantiation max number of bounds: " << FREEVAR_INSTANTIATE_MAXBOUNDS << endl;

    cout << " Farkas retry with extended guard:   ";
#ifdef FARKAS_TRY_ADDITIONAL_GUARD
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif

    cout << " Always try nonexecution of loops:   ";
#ifdef SELFLOOP_ALLOW_ZEROEXEC
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif

    cout << " Max loop nesting iterations:        " << NESTING_MAX_ITERATIONS << endl;

    cout << " Try chaining parallel selfloops:    ";
#ifdef NESTING_CHAIN_RANKED
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif

    cout << " Enable pruning to reduce runtime:   ";
#ifdef PRUNING_ENABLE
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif

    cout << " Pruning max # of parallel edges:    " << PRUNE_MAX_PARALLEL_TRANSITIONS << endl;

    cout << " Final infinity check:               ";
#ifdef FINAL_INFINITY_CHECK
    cout << "YES" << endl;
#else
    cout << "NO" << endl;
#endif
}


void printHelp(char *arg0) {
    cout << "Usage: " << arg0 << " [options] <file>" << endl;
    cout << "Options:" << endl;
    cout << "  --timeout <sec>    Timeout (in seconds), minimum: 10" << endl;
    cout << "  --dot <file>       Dump dot output to given file" << endl;
    cout << "  --stats            Print some statistics about the performed steps" << endl;
    cout << "  --timing           Print information about time usage" << endl;
    cout << "  --print-simplified Print simplfied program in the input format" << endl;
    cout << "  --allow-division   Allows division to occur in the input program" << endl;
    cout << "                     Note: LoAT is not sound for division in general" << endl;
    cout << "  --no-cost-check    Don't check if costs are nonnegative (potentially unsound)" << endl;
    cout << "  --no-preprocessing Don't try to simplify the program first (involves SMT)" << endl;
    cout << "  --limit-smt        Solve limit problems by SMT queries when applicable" << endl;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }

    //cmd options
    bool dotOutput = false;
    string dotFile;
    bool printStats = false;
    bool printTiming = false;
    bool printSimplified = false;
    bool allowDivision = false;
    bool checkCosts = true;
    bool doPreprocessing = true;
    bool limitSmtSolving = false;
    string filename;
    int timeout = 0;

    // ### Parse command line flags ###
    int arg=0;
    while (++arg < argc) {
        if (strcmp("--help",argv[arg]) == 0) {
            printHelp(argv[0]);
            return 1;
        }
        else if (strcmp("--dot",argv[arg]) == 0) {
            assert(arg < argc-1);
            dotOutput = true;
            dotFile = argv[++arg];
        }
        else if (strcmp("--timeout",argv[arg]) == 0) {
            assert(arg < argc-1);
            timeout = atoi(argv[++arg]);
        } else if (strcmp("--cfg",argv[arg]) == 0) {
            printConfig();
            return 1;
        } else if (strcmp("--stats",argv[arg]) == 0) {
            printStats = true;
        } else if (strcmp("--timing",argv[arg]) == 0) {
            printTiming = true;
        } else if (strcmp("--print-simplified",argv[arg]) == 0) {
            printSimplified = true;
        } else if (strcmp("--allow-division",argv[arg]) == 0) {
            allowDivision = true;
        } else if (strcmp("--no-preprocessing",argv[arg]) == 0) {
            doPreprocessing = false;
        } else if (strcmp("--no-cost-check",argv[arg]) == 0) {
            checkCosts = false;
        } else if (strcmp("--limit-smt",argv[arg]) == 0) {
            limitSmtSolving = true;
        } else {
            if (!filename.empty()) {
                cout << "Error: additional argument " << argv[arg] << " (already got filenam: " << filename << ")" << endl;
                return 1;
            }
            filename = argv[arg];
        }
    }
    if (filename.empty()) {
        cout << "Error: missing filename" << endl;
        return 1;
    }
    if (timeout > 0 && timeout < 10) {
        cout << "Error: timeout must be at least 10 seconds" << endl;
        return 1;
    }
    if (timeout > 0) {
        Timeout::setTimeouts(timeout);
    }

    if (allowDivision) {
        cout << endl << "WARNING: Allowing division in the input program can yield unsound results!" << endl;
        cout << "Division is only sound if the result of a term is always an integer" << endl << endl;
    }

    if (!checkCosts) {
        cout << endl << "WARNING: Not checking the costs can yield unsound results!" << endl;
        cout << "This is only safe if costs in the input program are guaranteed to be nonnegative" << endl << endl;
    }

    GlobalFlags::limitSmt = limitSmtSolving;

    // ### Start analyzing ###

    int dotStep=0;
    ofstream dotStream;
    if (dotOutput) {
        cout << "Trying to open dot output file: " << dotFile << endl;
        dotStream.open(dotFile);
        if (!dotStream.is_open()) {
            cout << "Error: Unable to open file: " << dotFile << endl;
            return 1;
        }
    }

    Timing::start(Timing::Total);
    cout << "Trying to load file: " << filename << endl;


    {
        ITSProblem its;
        using namespace parser;

        cout << "=== new ITSProblem ===" << endl;
        try {
            its = ITSParser::loadFromFile(filename, ITSParser::Settings());
        } catch (ITSParser::FileError err) {
            cerr << "Error: " << err.what() << endl;
            return 1;
        }

        cout << "SUCCESS" << endl << endl;
        its.print(cout);
        cout << "=== new ITSProblem ===" << endl;

        if (its.isLinear()) {
            LinearITSAnalysis::AnalysisSettings cfg(dotStream);
            auto runtime = LinearITSAnalysis::analyze(its, cfg);
            proofout << "Obtained the following complexity w.r.t. the length of the input n:" << endl;
            proofout << "  Complexity class: " << runtime.cpx << endl;
            proofout << "  Complexity value: ";
            {
                if (runtime.cpx.getType() == Complexity::CpxPolynomial) {
                    proofout << runtime.cpx.getPolynomialDegree().toFloat() << endl;
                } else {
                    proofout << runtime.cpx << endl;
                }
            }
        } else {
            NonlinearITSAnalysis::AnalysisSettings cfg(dotStream);
            auto runtime = NonlinearITSAnalysis::analyze(its, cfg);
            proofout << "Obtained the following complexity w.r.t. the length of the input n:" << endl;
            proofout << "  Complexity class: " << runtime.cpx << endl;
            proofout << "  Complexity value: ";
            {
                if (runtime.cpx.getType() == Complexity::CpxPolynomial) {
                    proofout << runtime.cpx.getPolynomialDegree().toFloat() << endl;
                } else {
                    proofout << runtime.cpx << endl;
                }
            }
        }
    }

    return 42;


    /*
    ITRSProblem res = ITRSProblem::loadFromFile(filename,allowDivision,checkCosts);
    FlowGraph g(res);

    proofout << endl << "Initial Control flow graph problem:" << endl;
    g.printForProof();
    if (dotOutput) g.printDot(dotStream,dotStep++,"Initial");

    if (g.reduceInitialTransitions()) {
        proofout << endl << "Removed unsatisfiable initial transitions:" << endl;
        g.printForProof();
        if (dotOutput) g.printDot(dotStream,dotStep++,"Reduced initial");
    }

    RuntimeResult runtime;
    if (!g.isEmpty()) {
        //do some preprocessing
        if (doPreprocessing) {
            if (g.preprocessTransitions(checkCosts)) {
                proofout << endl <<  "Simplified the transitions:" << endl;
                g.printForProof();
                if (dotOutput) g.printDot(dotStream,dotStep++,"Simplify");
            }
        }

        while (!g.isFullyChained()) {

            bool changed;
            do {
                changed = false;

                if (g.accelerateSimpleLoops()) {
                    changed = true;
                    proofout << endl <<  "Accelerated all simple loops using metering functions"
                             << " (where possible):" << endl;
                    g.printForProof();
                    if (dotOutput) g.printDot(dotStream,dotStep++,"Accelerate simple loops");
                }
                if (Timeout::soft()) break;

                if (g.chainSimpleLoops()) {
                    changed = true;
                    proofout << endl <<  "Chained simpled loops:" << endl;
                    g.printForProof();
                    if (dotOutput) g.printDot(dotStream,dotStep++,"Chain simple loops");
                }
                if (Timeout::soft()) break;

                if (g.chainLinear()) {
                    changed = true;
                    proofout << endl <<  "Eliminated locations (linear):" << endl;
                    g.printForProof();
                    if (dotOutput) g.printDot(dotStream,dotStep++,"Eliminate Locations (linear)");
                }
                if (Timeout::soft()) break;

            } while (changed);


            if (g.chainBranches()) {
                proofout << endl <<  "Eliminated locations (branches):" << endl;
                g.printForProof();
                if (dotOutput) g.printDot(dotStream,dotStep++,"Eliminate Locations (branches)");

            } else if (g.eliminateALocation()) {
                proofout << endl <<  "Eliminated locations:" << endl;
                g.printForProof();
                if (dotOutput) g.printDot(dotStream,dotStep++,"Eliminate Locations");
            }
            if (Timeout::soft()) break;

            if (g.pruneTransitions()) {
                proofout << endl <<  "Pruned:" << endl;
                g.printForProof();
                if (dotOutput) { g.printDot(dotStream,dotStep++,"Prune"); }
            }
            if (Timeout::soft()) break;
        }

        if (Timeout::soft()) proofout << "Aborted due to lack of remaining time" << endl << endl;

        //simplify the simplified program
        if (g.isFullyChained()) {
            g.removeDuplicateInitialTransitions();
        }

        proofout << endl << "Final control flow graph problem, now checking costs for infinitely many models:" << endl;
        g.printForProof();
        if (dotOutput) g.printDot(dotStream,dotStep++,"Final");

        if (printSimplified) {
            proofout << endl << "Simplified program in input format:" << endl;
            g.printKoAT();
            proofout << endl;
        }

        if (!g.isFullyChained()) {
            //handling for timeouts
            proofout << "This is only a partial result (probably due to a timeout), trying to find max complexity" << endl << endl;
            runtime = g.getMaxPartialResult();
        } else {
            //no timeout, fully contracted, find maximum runtime
            proofout << endl;
            runtime = g.getMaxRuntime();
        }

        //if we failed to prove a bound, we can still output O(1) with bound 1, as the graph was non-empty
        if (runtime.cpx == Complexity::Unknown) {
            runtime.cpx = Complexity::Const;
            runtime.bound = Expression(1);
            runtime.guard.clear();
        }
    }
    Timing::done(Timing::Total);

    // ### Some nice proof output ###

    proofout << endl;
    proofout << "The final runtime is determined by this resulting transition:" << endl;
    proofout << "  Final Guard: ";
    for (int i=0; i < runtime.guard.size(); ++i) { if (i > 0) proofout << " && "; proofout << runtime.guard.at(i); }
    proofout << endl;
    proofout << "  Final Cost:  " << runtime.bound << endl;
    proofout << endl;

    if (runtime.reducedCpx) {
        proofout << "Note that the variables in this cost term are only a sublinear fraction" << endl;
        proofout << "of the input length n, so the complexity had to be reduced." << endl << endl;
    }

    proofout << "Obtained the following complexity w.r.t. the length of the input n:" << endl;
    proofout << "  Complexity class: " << runtime.cpx << endl;
    proofout << "  Complexity value: ";
    {
        if (runtime.cpx.getType() == Complexity::CpxPolynomial) {
            proofout << runtime.cpx.getPolynomialDegree().toFloat() << endl;
        } else {
            proofout << runtime.cpx << endl;
        }
    }

#ifndef DEBUG_DISABLE_ALL
    cout << "DEBUG Bound: " << runtime.bound << endl;
    cout << "DEBUG Complexity value: ";
    {
        if (runtime.cpx.getType() == Complexity::CpxPolynomial) {
            cout << runtime.cpx.getPolynomialDegree().toFloat() << endl;
        } else {
            cout << runtime.cpx << endl;
        }
    }
#endif

    if (dotOutput) {
        g.printDotText(dotStream,dotStep++,runtime.cpx.toString());
        dotStream << "}" << endl;
        dotStream.close();
    }

    if (printStats) {
        cout << endl;
        Stats::print(cout);
    }

    if (printTiming) {
        cout << endl;
        Timing::print(cout);
    }

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
    */
}

