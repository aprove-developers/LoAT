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

#ifndef DEBUG_H
#define DEBUG_H


/* ### Debugging related includes ### */

#include <iostream>
#include <ostream>
#include <cassert>


/* ### Global debugging output flag ### */

#define DEBUG_DISABLE_ALL


/* ### Make sure assertions are enabled ### */

#ifdef NDEBUG
#undef NDEBUG
#endif


/* ### Some special assertions ### */

#define unreachable() assert(false && "unreachable")
#define unimplemented() assert(false && "not implemented")


/* ### Colored debugging output */

#ifdef COLORS_DEBUG
    #define COLOR_WARN "\033[1;31m" // bold red
    #define COLOR_PROBLEM "\033[1;33m" // bold yellow
    #define COLOR_DEBUG "\033[0;90m" // gray/bright black (avoid distraction from proof output)
    #define COLOR_HIGHLIGHT "\033[0;36m" // cyan (to be quickly visible)
    #define COLOR_NONE "\033[0m" // reset color to default
#else
    #define COLOR_WARN ""
    #define COLOR_PROBLEM ""
    #define COLOR_DEBUG ""
    #define COLOR_HIGHLIGHT ""
    #define COLOR_NONE ""
#endif


/* ### Individual debugging flags ### */

#ifndef DEBUG_DISABLE_ALL

//print an overview of the problem after every relevant simplification step (similar to proof output)
//#define DEBUG_PRINTSTEPS

//print problematic results that (might) have a strong impact on the result
#define DEBUG_PROBLEMS

//print warnings which might indicate bugs
#define DEBUG_WARN

//dump z3 solvers and results
//#define DEBUG_Z3

//debugging for the conversion from GiNaC expressions to z3 expressions
//#define DEBUG_GINACTOZ3

//debugging for the graph datastructure
#define DEBUG_GRAPH

//debugging for the analysis of linear ITS problems
#define DEBUG_LINEAR
#define DEBUG_CHAINING
#define DEBUG_ACCELERATE
#define DEBUG_PRUNING

// debugging for metering
#define DEBUG_METERING

// TODO: refine FARKAS debugging to refactored code (Farkas, Metering, Linearize etc.)
//debugging for Farkas processor
#define DEBUG_FARKAS

//debugging for final infinity check
#define DEBUG_INFINITY

//debugging for limit problems
#define DEBUG_LIMIT_PROBLEMS

//debugging for asymptotic bounds
#define DEBUG_ASYMPTOTIC_BOUNDS

//debugging for the Expression class
#define DEBUG_EXPRESSION

//debugging for the ITS problem parser
#define DEBUG_PARSER

//debugging for the term parser (part of the ITS problem parser)
//#define DEBUG_TERM_PARSER

//debugging for PURRS
#define DEBUG_PURRS

//debugging for all other parts
#define DEBUG_OTHER

#endif


/* ### Debugging macros ### */

//useful for short fixes/debugging tests:
#define debugTest(output) do { std::cout << COLOR_HIGHLIGHT << "[test] " << output << COLOR_NONE << std::endl; } while(0)

#define dumpList(desc,guard) do {\
    std::cout << COLOR_DEBUG << "  [dump] " << desc << ":"; \
    for (const auto& x : (guard)) std::cout << " " << x; \
    std::cout << COLOR_NONE << std::endl; \
} while(0)

#define dumpMap(desc,update) do {\
    std::cout << COLOR_DEBUG << "  [dump] " << desc << ":"; \
    for (auto const &it : (update)) std::cout << " " << it.first << "=" << it.second; \
    std::cout << COLOR_NONE << std::endl; \
} while(0)

#define dumpMaps(desc,updates) do {\
    std::cout << COLOR_DEBUG << "  [dump] " << desc << ":"; \
    for (int i=0; i < (updates).size(); ++i) { \
        std::cout << std::endl << "     [" << i << "]"; \
        for (auto const &it : (updates)[i]) std::cout << " " << it.first << "=" << it.second; \
    } \
    std::cout << COLOR_NONE << std::endl; \
} while(0)

#define dumpGuardUpdate(desc,guard,update) do {\
    std::cout << COLOR_DEBUG << "  [dump] " << desc << ":" << std::endl; \
    dumpList("   guard",guard); \
    dumpMap("  update",update); \
} while(0)

#define dumpGuardUpdates(desc,guard,updates) do {\
    std::cout << COLOR_DEBUG << "  [dump] " << desc << ":" << std::endl; \
    dumpList("   guard",guard); \
    dumpMaps("  update",updates); \
} while(0)


#ifdef DEBUG_Z3
#define debugZ3(solver,res,location) do {\
    std::cout << COLOR_DEBUG << "[z3] " << (location) << " Z3 Solver: " << (solver);\
    std::cout << "Z3 Result: " << (res) << COLOR_NONE << std::endl;\
} while(0)
#else
#define debugZ3(solver,res,location) (void(0))
#endif

#ifdef DEBUG_GINACTOZ3
#define debugGinacToZ3(output) do { std::cout << COLOR_DEBUG << "[ginac-z3] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugGinacToZ3(output) (void(0))
#endif

#ifdef DEBUG_PURRS
#define debugPurrs(output) do { std::cout << COLOR_DEBUG << "[purrs] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugPurrs(output) (void(0))
#endif

#ifdef DEBUG_GRAPH
#define debugGraph(output) do { std::cout << COLOR_DEBUG << "[graph] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugGraph(output) (void(0))
#endif

#ifdef DEBUG_LINEAR
#define debugLinear(output) do { std::cout << COLOR_HIGHLIGHT << "[linear] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugLinear(output) (void(0))
#endif

#ifdef DEBUG_CHAINING
#define debugChain(output) do { std::cout << COLOR_HIGHLIGHT << "[chain] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugChain(output) (void(0))
#endif

#ifdef DEBUG_PRUNING
#define debugPrune(output) do { std::cout << COLOR_HIGHLIGHT << "[prune] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugPrune(output) (void(0))
#endif

#ifdef DEBUG_ACCELERATE
#define debugAccel(output) do { std::cout << COLOR_HIGHLIGHT << "[accelerate] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugAccel(output) (void(0))
#endif

#ifdef DEBUG_METERING
#define debugMeter(output) do { std::cout << COLOR_HIGHLIGHT << "[meter] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugMeter(output) (void(0))
#endif

#ifdef DEBUG_FARKAS
#define debugFarkas(output) do { std::cout << COLOR_DEBUG << "[farkas] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugFarkas(output) (void(0))
#endif

#ifdef DEBUG_INFINITY
#define debugInfinity(output) do { std::cout << COLOR_DEBUG << "[infinity] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugInfinity(output) (void(0))
#endif

#ifdef DEBUG_LIMIT_PROBLEMS
#define debugLimitProblem(output) do { std::cout << COLOR_DEBUG << "[limitproblem] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugLimitProblem(output) (void(0))
#endif

#ifdef DEBUG_ASYMPTOTIC_BOUNDS
#define debugAsymptoticBound(output) do { std::cout << COLOR_DEBUG << "[asymptotic] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugAsymptoticBound(output) (void(0))
#endif

#ifdef DEBUG_PROBLEMS
#define debugProblem(output) do { std::cout << COLOR_PROBLEM << "[PROBLEM] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugProblem(output) (void(0))
#endif

#ifdef DEBUG_PARSER
#define debugParser(output) do { std::cout << COLOR_DEBUG << "[parser] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugParser(output) (void(0))
#endif

#ifdef DEBUG_TERM_PARSER
#define debugTermParser(output) do { std::cout << COLOR_DEBUG << "[termparser] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugTermParser(output) (void(0))
#endif

#ifdef DEBUG_OTHER
#define debugOther(output) do { std::cout << COLOR_DEBUG << "[other] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugOther(output) (void(0))
#endif

#ifdef DEBUG_WARN
#define debugWarn(output) do { std::cout << COLOR_WARN << "[WARNING] " << output << COLOR_NONE << std::endl; } while(0)
#else
#define debugWarn(output) (void(0))
#endif


#endif
