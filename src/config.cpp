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

#include "config.hpp"
#include <iostream>

using namespace std;

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

    namespace Output {
        // Whether to enable colors in the proof output
        bool Colors = true;

        // Whether to use colors in ITSExport (to distinguish guard/update/cost)
        bool ColorsInITS = true;

        // Whether to include timestamps in every line of the proof output
        bool Timestamps = false;

        // Whether to print the fully simplified ITS problem in the input format (KoAT's format).
        // Mostly useful for debugging the asymptotic check.
        bool ExportSimplified = false;

        // If present, a dot file is generated with a subgraph after every relevant step.
        // This is only applicable to linear ITS problems and often gets very large.
        option<string> DotFile = {};

        // Include all chaining steps in the proof output?
        bool ProofChain = false;

        // Include all solved limit problems in the proof output?
        bool ProofLimit = true;
    }

    namespace Color {
        // Proof output
        const std::string Section = "\033[1;33m"; // bold yellow
        const std::string Headline = "\033[1;34m"; // bold blue
        const std::string Warning = "\033[1;31m"; // bold red
        const std::string Result = "\033[1;32m"; // bold green
        const std::string None = "\033[0m"; // reset color

        // ITS Output
        const std::string Location = "\033[0;34m"; // bold blue
        const std::string Update = "\033[0;36m"; // cyan
        const std::string Guard = "\033[0;32m"; // green
        const std::string Cost = "\033[0;33m"; // yellow

        // Debugging
        const std::string Debug = "\033[0;90m"; // gray/bright black (avoid distraction)
        const std::string DebugProblem = "\033[1;33m"; // bold yellow
        const std::string DebugWarning = "\033[1;31m"; // bold red
        const std::string DebugHighlight = "\033[0;36m"; // cyan
    }

    namespace Parser {
        // Whether to allow division to occur in the input.
        // NOTE: Settings this to true can be unsound (if some terms in the input do not map to int)!
        bool AllowDivision = false;
    }

    namespace Z3 {
        // Timeouts (default / for metering / for limit smt encoding)
        const unsigned DefaultTimeout = 500u;
        const unsigned MeterTimeout = 500u;
        const unsigned StrengtheningTimeout = 750u;
        const unsigned LimitTimeout = 500u;
        const unsigned LimitTimeoutFinal = 3000u;
        const unsigned LimitTimeoutFinalFast = 500u;

        // The largest k for which x^k is rewritten to x*x*...*x (k times).
        // z3 does not like powers, so writing x*x*...*x can sometimes help.
        const unsigned MaxExponentWithoutPow = 5;
    }

    namespace ForwardAccel {
        // Whether to allow rational coefficients when searching for a metering function.
        // In the paper this is always sound. For the implementation, we take special
        // care of rational coefficients since we want that all guard constraints map to int.
        bool AllowRealCoeffs = true;

        // Whether to perform linearization if the guard/update has nonlinear arithmetic.
        // This can help to still infer a linear metering function (linear in the substituted variables).
        // Disabling is mostly relevant for benchmarking (this is not described in the paper) and for debugging.
        bool AllowLinearization = true;

        // Whether to allow conditional metering functions.
        // Note that we always allow to reduce the guard (drop constraints irrelevant for termination).
        // The only difference if this is enabled is that we add the irrelevent guard to the lhs
        // when considering the constraint "not guard  ==>  b <= 0"
        bool ConditionalMetering = true;

        // Instantiate temp vars by all possible bounds found in the guard if metering failed?
        // We currently try to instantiate all at the same time, so this can be expensive.
        bool TempVarInstantiation = true;

        // The maximum number of bounds that are tried for a single free variable
        // when instantiating temporary variables (this limit is there to prevent exponential complexity).
        const unsigned TempVarInstantiationMaxBounds = 3;

        // Heuristic to workaround missing min(A,B), e.g. for "X <= A && X <= B".
        // The heuristic adds "A >= B" or "A <= B" to the guard, which allows finding a metering function.
        bool ConflictVarHeuristic = true;

        // Heuristic to workaround updates like "A := 0" or "A := free" (update rhs has no updated variable).
        // We often fail to find a metering function. The heuristic propagates "A/0" or "A/free" to the guard
        // by extending the guard. This restricts the rule, but might allow us to find a metering function.
        bool ConstantUpdateHeuristic = true;

        // If true, iterated update/cost are computed for a fresh variable "tv"
        // and the constraint "0 < tv <= meter" is added to the guard (as in the paper).
        // If false, iterated update/cost are directly computed with "meter"
        // (this means that we assume loops are always fully executed).
        // The second option produces much simpler rules and is thus the current default.
        const bool UseTempVarForIterationCount = false;
    }

    // Backward acceleration technique
    namespace BackwardAccel {
        // Backward iteration uses a fresh var "k" for the iteration count.
        // If true, then "k" is replaced by its upper bounds from the guard (if possible).
        const bool ReplaceTempVarByUpperbounds = true;

        // If KeepTempVarForIterationCount is false, then "k" is instantiated by its upper bounds.
        // If there are several upperbounds, several rules are created.
        // To avoid rule explosion, the propagation is only performed up to this number of upperbounds.
        const unsigned MaxUpperboundsForPropagation = 3;
    }

    namespace Accel {
        // Simplify rules before trying to accelerate them.
        // Should be enabled (especially helps to eliminate free variables).
        const bool SimplifyRulesBefore = true;

        // Heuristic to shorten nonlinear rules by deleting some of the rhss if metering failed.
        // We currently try to meter all pairs and every single rhs, so this is rather expensive.
        bool PartialDeletionHeuristic = true;

        // If true, tries to nest parallel rules. Nesting means that one iteration of the "outer"
        // loop is followed by a full execution of the "inner" loop. This is a simple application
        // of chaining combined with acceleration, but is not described in the paper.
        bool TryNesting = true;
    }

    // Chaining and chaining strategies
    namespace Chain {
        // Whether to check if the chained rule's guard is still satisfiable.
        // This is expensive if many rules are chained, but should nevertheless be enabled.
        // If disabled, a rule with high complexity can become unsatisfiable if chained with an incompatible rule.
        // If disabled, many unsatisfiable rules could be created, leading to rule explosion.
        const bool CheckSat = true;

        // Whether to keep incoming rules after chaining them with accelerated rules.
        // In many cases, we don't need the incoming rules if they have been successfully chained.
        // But there are cases where some loops are not yet simple, so we accelerate them at a later point in time.
        // For such cases, it helps to keep the incoming rules (even if this increases the number of rules).
        const bool KeepIncomingInChainAccelerated = true;
    }

    namespace Prune {
        // Prune parallel rules if there are more than this number.
        // We consider two rules parallel if they have an edge in common, e.g. f -> f,g and f -> g are parallel.
        const unsigned MaxParallelRules = 5;
    }

    // Asymptotic complexity computation using limit problems
    namespace Limit {
        PolynomialLimitProblemStrategy* PolyStrategy = &SmtAndCalculus;

        // Discard a limit problem of size >= ProblemDiscardSize in a non-final check if z3 yields "unknown"
        const unsigned int ProblemDiscardSize = 10;
    }

    namespace Analysis {
        // Simplify the rules before starting the analysis?
        // This involves z3 (to find unsat rules and to simplify guards) and might be expensive.
        // Disabling is especially useful when debugging specific examples.
        bool Preprocessing = true;

        // Whether to enable pruning to reduce the number of rules.
        // Pruning works by greedily keeping rules with a high complexity.
        // To be more accurate, this involves the asymptotic check (and can thus be expensive).
        bool Pruning = true;

        // Whether a constraint "cost >= 0" is added to every rule.
        // This influences the semantics: If false, rules can be taken even if the cost is negative.
        bool EnsureNonnegativeCosts = true;

        // Whether to check for constant complexity (reachable satisfiable rule with cost >= 1).
        // If disabled, Omega(0) is reported if no non-constant complexity can be inferred.
        // If enabled, a heuristic is used that only checks initial rules to prove Omega(1).
        // Involves SMT queries and can impact performance (even if a higher complexity is inferred).
        bool ConstantCpxCheck = true;

        // Whether to enable the asymptotic complexity computation at all.
        // NOTE: It is not sound to disable this (only intended for testing!)
        // NOTE: If disabled, the complexity is only approximated by a syntactic check.
        bool AsymptoticCheck = true;

        bool NonTermMode = false;
    }

}

#define GetColor(a) ((Config::Output::Colors) ? (Config::Color::a) : "")

#define PrintCfg(a,b) \
    os << #a << " = " << a; \
    if (withDescription) os << GetColor(Debug) << "  // " << b << GetColor(None); \
    os << endl

void Config::printConfig(ostream &os, bool withDescription) {
    auto startSection = [&](const string &s) {
        os << endl << GetColor(Headline) << "## " << s << " ##" << GetColor(None) << endl;
    };

    os << "LoAT Configuration" << endl;

    {
        using namespace Config::Output;
        startSection("Output");
        PrintCfg(Colors, "Enable colors in proof output");
        PrintCfg(ColorsInITS, "Enable colors when printing ITS problems");
        PrintCfg(Timestamps, "Include timestamps in the proof output");
        PrintCfg(ExportSimplified, "Print simplified ITS problem in input format");
        PrintCfg(DotFile, "Path for dot export (for linear ITS problems only)");
        PrintCfg(ProofChain, "Print all chaining steps");
        PrintCfg(ProofLimit, "Print steps when solving limit problems");
    }
    {
        using namespace Config::Parser;
        startSection("Parser");
        PrintCfg(AllowDivision, "Allow divisions in the input file (currently not sound!)");
    }
    {
        using namespace Config::Z3;
        startSection("Z3");
        PrintCfg(DefaultTimeout, "Timeout for most z3 calls");
        PrintCfg(MeterTimeout, "Timeout for z3 when searching for metering functions");
        PrintCfg(LimitTimeout, "Timeout for z3 when solve limit problems via smt encoding");
        PrintCfg(MaxExponentWithoutPow, "Max degree for rewriting powers as products for Z3");
    }
    {
        using namespace Config::ForwardAccel;
        startSection("Forward Acceleration");
        PrintCfg(AllowRealCoeffs, "Allow rational coefficients in metering functions");
        PrintCfg(AllowLinearization, "Linearize guard/update by temporary substitutions");
        PrintCfg(ConditionalMetering, "Include the irrelevant guard for conditional metering");
        PrintCfg(TempVarInstantiation, "Instantiate temporary variables by their bounds");
        PrintCfg(TempVarInstantiationMaxBounds, "Max bounds to try when instantiating temp vars");
        PrintCfg(ConflictVarHeuristic, "Workaround for cases with metering function min(A,B)");
        PrintCfg(ConstantUpdateHeuristic, "Heuristic for updates with constant right-hand side");
        PrintCfg(UseTempVarForIterationCount, "Use 1 <= tv <= meter or instantiate tv by meter");
    }
    {
        using namespace Config::BackwardAccel;
        startSection("Backward Acceleration");
        PrintCfg(ReplaceTempVarByUpperbounds, "Replace iteration count by its upper bounds");
        PrintCfg(MaxUpperboundsForPropagation, "Max number of upper bounds to allow when replacing");
    }
    {
        using namespace Config::Accel;
        startSection("Acceleration");
        PrintCfg(SimplifyRulesBefore, "Simplify simple loops before acceleration");
        PrintCfg(PartialDeletionHeuristic, "Apply partial deletion if acceleration fails");
        PrintCfg(TryNesting, "Try to interpret parallel simple loops as nested loops");
    }
    {
        using namespace Config::Chain;
        startSection("Chaining");
        PrintCfg(CheckSat, "Only chain if the resulting chained rule is satisfiable");
        PrintCfg(KeepIncomingInChainAccelerated, "Keep incoming rules after chaining with accelerated rules");
    }
    {
        using namespace Config::Prune;
        startSection("Pruning");
        PrintCfg(MaxParallelRules, "Number of parallel rules for which pruning is applied");
    }
    {
        using namespace Config::Limit;
        startSection("Limit Problems");
        PrintCfg(PolyStrategy, "Strategy to solve limit problems");
        PrintCfg(ProblemDiscardSize, "Discard problems of this size if z3 says unknown");
    }
    {
        using namespace Config::Analysis;
        startSection("Main Algorithm");
        PrintCfg(Preprocessing, "Perform several pre-processing steps to simplify rules");
        PrintCfg(Pruning, "Whether to enable pruning of rules");
        PrintCfg(EnsureNonnegativeCosts, "Add 'cost >= 0' to all guards, disallow rules with negative costs");
        PrintCfg(AsymptoticCheck, "Use asymptotic complexity computation (must be enabled for soundness!)");
    }
}
