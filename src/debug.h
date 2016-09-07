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

//#define DEBUG_DISABLE_ALL


/* ### Make sure assertions are enabled ### */

#ifdef NDEBUG
#undef NDEBUG
#endif


/* ### Some special assertions ### */

#define unreachable() assert(false)
#define unimplemented() assert(false)


/* ### Define a dummy ostream object ### */

struct DebugStream {
    static std::ostream dummy;
};


/* ### Define for the proof output (can be used to disable proof output) ### */

#define proofout cout
//#define proofout DebugStream::dummy


/* ### Individual debugging flags ### */

#ifndef DEBUG_DISABLE_ALL

//print an overview of the problem after every relevant simplification step (similar to proof output)
//#define DEBUG_PRINTSTEPS

//print problems that (might) have a strong impact on the result
#define DEBUG_PROBLEMS

//dump z3 solvers and results
//#define DEBUG_Z3

//debugging for FlowGraph and the underlying Graph class
#define DEBUG_GRAPH

//debugging for RecursionGraph
#define DEBUG_RECURSION_GRAPH

//debugging for Farkas processor
#define DEBUG_FARKAS

//debugging for final infinity check
#define DEBUG_INFINITY

//debugging for limit problems
//#define DEBUG_LIMIT_PROBLEMS

//debugging for asymptotic bounds
//#define DEBUG_ASYMPTOTIC_BOUNDS

//debugging for the Expression class
#define DEBUG_EXPRESSION

//debugging for the Term class
//#define DEBUG_TERM

//debugging for the ITS problem parser
#define DEBUG_PARSER

//debugging for the ITRS term parser
//#define DEBUG_TERM_PARSER

//debugging for recursion solving
#define DEBUG_RECURSION

//debugging for PURRS
//#define DEBUG_PURRS

//debugging for all other parts
#define DEBUG_OTHER

#endif


/* ### Debugging macros ### */

//useful for short fixes/debugging tests:
#define debugTest(output) do { std::cerr << output << std::endl; } while(0)

#define dumpList(desc,guard) do {\
    cout << desc << ":"; for (const auto& x : (guard)) cout << " " << x; cout << endl; \
} while(0)

#define dumpMap(desc,update) do {\
    cout << desc << ":"; for (auto const &it : (update)) cout << " " << it.first << "=" << it.second; cout << endl; \
} while(0)

#define dumpGuardUpdate(desc,guard,update) do {\
    cout << desc << ":" << endl; \
    dumpList(" guard",guard); \
    dumpMap("update:",update); \
} while(0)

#ifdef DEBUG_Z3
#define debugZ3(solver,res,location) do { std::cout << (location) << " Z3 Solver: " << (solver) << "Z3 Result: " << (res) << std::endl; } while(0)
#else
#define debugZ3(solver,res,location) (void(0))
#endif

#ifdef DEBUG_RECURSION
#define debugRecursion(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugRecursion(output) (void(0))
#endif

#ifdef DEBUG_PURRS
#define debugPurrs(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugPurrs(output) (void(0))
#endif

#ifdef DEBUG_GRAPH
#define debugGraph(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugGraph(output) (void(0))
#endif

#ifdef DEBUG_RECURSION_GRAPH
#define debugRecGraph(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugRecGraph(output) (void(0))
#endif

#ifdef DEBUG_FARKAS
#define debugFarkas(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugFarkas(output) (void(0))
#endif

#ifdef DEBUG_INFINITY
#define debugInfinity(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugInfinity(output) (void(0))
#endif

#ifdef DEBUG_LIMIT_PROBLEMS
#define debugLimitProblem(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugLimitProblem(output) (void(0))
#endif

#ifdef DEBUG_ASYMPTOTIC_BOUNDS
#define debugAsymptoticBound(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugAsymptoticBound(output) (void(0))
#endif

#ifdef DEBUG_PROBLEMS
#define debugProblem(output) do { std::cerr << output << std::endl; } while(0)
#else
#define debugProblem(output) (void(0))
#endif

#ifdef DEBUG_TERM
#define debugTerm(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugTerm(output) (void(0))
#endif

#ifdef DEBUG_PARSER
#define debugParser(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugParser(output) (void(0))
#endif

#ifdef DEBUG_TERM_PARSER
#define debugTermParser(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugTermParser(output) (void(0))
#endif

#ifdef DEBUG_OTHER
#define debugOther(output) do { std::cout << output << std::endl; } while(0)
#else
#define debugOther(output) (void(0))
#endif

#endif
