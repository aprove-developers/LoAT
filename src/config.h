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

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <ostream>

/**
 * Global settings and constants.
 *
 * Variables which are "const" should mostly not be changed.
 * The other variables may be changed (e.g., to choose which heuristics are used)
 * and may be controlled via command line options.
 *
 * See the source file for documentation (since this is also where the default values are defined)
 */
namespace Config {

    // Proof output
    namespace Output {
        extern bool Colors;
        extern bool ColorsInITS;
        extern bool Timestamps;
        extern bool DotOutput;
        extern bool ExportSimplified;
        extern bool ProofMeter;
        extern bool ProofChain;
        extern bool ProofLimit;
    }

    // Colors (Ansi color codes) for output
    namespace Color {
        extern const std::string Section;
        extern const std::string Headline;
        extern const std::string Warning;
        extern const std::string Result;
        extern const std::string None;

        extern const std::string Location;
        extern const std::string Update;
        extern const std::string Guard;
        extern const std::string Cost;

        extern const std::string Debug;
        extern const std::string DebugProblem;
        extern const std::string DebugWarning;
        extern const std::string DebugHighlight;
    }

    // All settings for interfacing z3
    namespace Z3 {
        extern const unsigned DefaultTimeout;
        extern const unsigned MeterTimeout;
        extern const unsigned LimitTimeout;
        extern const unsigned MaxExponentWithoutPow;
    }

    // Acceleration with metering functions
    namespace ForwardAccel {
        extern bool AllowRealCoeffs;
        extern bool AllowLinearization;
        extern bool ConditionalMetering;
        extern bool TempVarInstantiation;
        extern bool ConflictVarHeuristic;
        extern bool ConstantUpdateHeuristic;
        extern const unsigned TempVarInstantiationMaxBounds;
        // TODO: Not yet implemented
        // extern const bool UseTempVarForIterationCount;
    }

    // Backward acceleration technique
    namespace BackwardAccel {
        extern const bool KeepTempVarForIterationCount;
        extern const unsigned MaxUpperboundsForPropagation;
    }

    // High level acceleration strategy
    namespace Accel {
        extern const bool SimplifyRulesBefore;
        extern bool PartialDeletionHeuristic;
        extern bool UseBackwardAccel;

        // TODO: Not yet implemented
        // extern const bool ChainAcceleratedRulesAmongThemselves;
    }

    // Chaining and chaining strategies
    namespace Chain {
        extern const bool CheckSat;
        extern const bool KeepIncomingInChainAccelerated;
    }

    // Pruning in case of too many rules
    namespace Prune {
        extern const unsigned MaxParallelRules;
    }

    // Asymptotic complexity computation using limit problems
    namespace Limit {
        extern bool UseSmtEncoding;
        extern const int ProblemDiscardSize;
    }

    // Parser for ITS problems
    namespace Parser {
        extern bool AllowDivision;
    }

    // Main algorithm
    namespace Analysis {
        extern bool Preprocessing;
        extern bool Pruning;
        extern bool EnsureNonnegativeCosts;
        extern bool AsymptoticCheck; // must be enabled for soundness!
    }

    /**
     * Prints all of the above config values to the given stream.
     * Useful to test command line flags and to include configuration benchmarks.
     */
    //void printConfig(std::ostream &s, bool withDescription);
}

#endif //CONFIG_H
