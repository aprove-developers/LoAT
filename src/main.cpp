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


#include "analysis/preprocess.h"
#include "util/stats.h"
#include "util/timing.h"
#include "util/timeout.h"

#include "its/parser/itsparser.h"
#include "analysis/analysis.h"

#include "config.h"



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

bool allowRecursion = false;

void setupConfig(bool conditionalMeter, bool backAccel, bool recursion, bool limitSmt) {
    Config::ForwardAccel::ConditionalMetering = conditionalMeter;
    Config::Accel::UseBackwardAccel = backAccel;
    allowRecursion = recursion;
    Config::Limit::UseSmtEncoding = limitSmt;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " --bench a/b/c/d/e/f --timeout N [--paper] <file>";
        return 1;
    }

    int timeout;
    char benchmode;
    bool disableHeuristics = false;
    string filename;

    // ### Parse command line flags ###
    int arg=0;
    while (++arg < argc) {
        if (strcmp("--timeout",argv[arg]) == 0) {
            assert(arg < argc-1);
            timeout = atoi(argv[++arg]);
        } else if (strcmp("--paper",argv[arg]) == 0) {
            disableHeuristics = true;
        } else if (strcmp("--bench",argv[arg]) == 0) {
            assert(arg < argc-1);
            assert(strlen(argv[arg+1]) == 1);
            benchmode = argv[++arg][0];
        } else {
            if (!filename.empty()) {
                cout << "Error: additional argument " << argv[arg] << " (already got: " << filename << ")" << endl;
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

    if (timeout <= 0) {
        cout << "Error: timeout required for benchmarking" << endl;
        return 1;
    }

    switch (benchmode) {
        case 'a': setupConfig(false, false, false, false); break;
        case 'b': setupConfig(true,  false, false, false); break;
        case 'c': setupConfig(false, true,  false, false); break;
        case 'd': setupConfig(false, false, true,  false); break;
        case 'e': setupConfig(false, false, false, true); break;
        case 'f': setupConfig(true,  true,  true,  true); break;
        default: cout << "Unknown benchmark setting" << endl; return 1;
    }

    if (disableHeuristics) {
        Config::ForwardAccel::AllowLinearization = false;
        Config::ForwardAccel::ConflictVarHeuristic = false;
        Config::ForwardAccel::ConstantUpdateHeuristic = false;
    }

    Timeout::setTimeouts(timeout);

    // ### Start analyzing ###

    Timing::start(Timing::Total);
    cout << "Trying to load file: " << filename << endl;

    {
        ITSProblem its;
        using namespace parser;

        cout << "=== new ITSProblem ===" << endl;
        try {
            its = ITSParser::loadFromFile(filename);
        } catch (ITSParser::FileError err) {
            cerr << "Error: " << err.what() << endl;
            return 1;
        }

        cout << "SUCCESS" << endl << endl;
        its.print(cout);
        cout << "=== new ITSProblem ===" << endl;

        RuntimeResult runtime;

        // Skip ITS problems with nonlinear (i.e., recursive) rules.
        if (allowRecursion || its.isLinear()) {
            runtime = Analysis::analyze(its);
        }

        // Alternative: Remove nonlinear (recursive) rules.
/*        if (!allowRecursion) {
            for (TransIdx trans : its.getAllTransitions()) {
                if (!its.getRule(trans).isLinear()) {
                    its.removeRule(trans);
                }
            }
        }
        runtime = Analysis::analyze(its);
        */

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

        proofout << endl;

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

