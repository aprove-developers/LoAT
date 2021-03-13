cmake_minimum_required(VERSION 3.7)

project(LoAT)

set(CMAKE_CXX_STANDARD 17)

# non-static build by default
option(STATIC "static" OFF)

if(${STATIC})
    message(STATUS "Configuring static build")
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    set(BUILD_SHARED_LIBS OFF)
    set(LINKER_OPTIONS "-static -ldl")
    set(EXECUTABLE loat-static)
else()
    message(STATUS "Configuring non-static build")
    set(LINKER_OPTIONS "")
    set(EXECUTABLE loat)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic -std=c11 -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "Compiler flags:" ${CMAKE_CXX_COMPILE_FLAGS})
message(STATUS "Compiler cxx debug flags:" ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "Compiler cxx release flags:" ${CMAKE_CXX_FLAGS_RELEASE})
message(STATUS "Compiler cxx min size flags:" ${CMAKE_CXX_FLAGS_MINSIZEREL})
message(STATUS "Compiler cxx flags:" ${CMAKE_CXX_FLAGS})

add_executable(${EXECUTABLE}
        src/accelerate/meter/metering.cpp
        src/accelerate/meter/metering.hpp
        src/accelerate/meter/metertools.cpp
        src/accelerate/meter/metertools.hpp
        src/accelerate/recurrence/dependencyorder.cpp
        src/accelerate/recurrence/dependencyorder.hpp
        src/accelerate/recurrence/recurrence.cpp
        src/accelerate/recurrence/recurrence.hpp
        src/accelerate/accelerator.cpp
        src/accelerate/accelerator.hpp
        src/accelerate/loopacceleration.cpp
        src/accelerate/loopacceleration.hpp
        src/accelerate/recursionacceleration.cpp
        src/accelerate/recursionacceleration.hpp
        src/accelerate/result.hpp
        src/accelerate/iterationCounterElimination/boundextractor.hpp
        src/accelerate/iterationCounterElimination/boundextractor.cpp
        src/accelerate/iterationCounterElimination/vareliminator.cpp
        src/accelerate/iterationCounterElimination/vareliminator.hpp
        src/accelerate/accelerationCalculus/accelerationproblem.cpp
        src/accelerate/accelerationCalculus/accelerationproblem.hpp
        src/analysis/analysis.cpp
        src/analysis/analysis.hpp
        src/analysis/chain.cpp
        src/analysis/chain.hpp
        src/analysis/chainstrategy.cpp
        src/analysis/chainstrategy.hpp
        src/analysis/preprocess.cpp
        src/analysis/preprocess.hpp
        src/analysis/prune.cpp
        src/analysis/prune.hpp
        src/asymptotic/asymptoticbound.cpp
        src/asymptotic/asymptoticbound.hpp
        src/asymptotic/inftyexpression.cpp
        src/asymptotic/inftyexpression.hpp
        src/asymptotic/limitproblem.cpp
        src/asymptotic/limitproblem.hpp
        src/asymptotic/limitsmt.cpp
        src/asymptotic/limitsmt.hpp
        src/asymptotic/limitvector.cpp
        src/asymptotic/limitvector.hpp
        src/expr/complexity.cpp
        src/expr/complexity.hpp
        src/expr/expression.cpp
        src/expr/expression.hpp
        src/expr/rel.cpp
        src/expr/rel.hpp
        src/expr/guardtoolbox.cpp
        src/expr/guardtoolbox.hpp
        src/expr/boolexpr.cpp
        src/expr/boolexpr.hpp
        src/its/koatParser/itsparser.cpp
        src/its/koatParser/itsparser.hpp
        src/its/koatParser/term.cpp
        src/its/koatParser/term.hpp
        src/its/koatParser/termparser.cpp
        src/its/koatParser/termparser.hpp
        src/its/export.cpp
        src/its/export.hpp
        src/its/hypergraph.hpp
        src/its/itsproblem.cpp
        src/its/itsproblem.hpp
        src/its/rule.cpp
        src/its/rule.hpp
        src/its/types.hpp
        src/its/guard.cpp
        src/its/guard.hpp
        src/its/variablemanager.cpp
        src/its/variablemanager.hpp
        src/util/exceptions.hpp
        src/util/option.hpp
        src/util/proof.hpp
        src/util/proof.cpp
        src/util/timeout.cpp
        src/util/timeout.hpp
        src/util/status.hpp
        src/util/farkas.cpp
        src/util/farkas.hpp
        src/util/templates.cpp
        src/util/templates.hpp
        src/util/relevantvariables.cpp
        src/util/relevantvariables.hpp
        src/config.cpp
        src/config.hpp
        src/main.cpp
        src/main.hpp
        src/nonterm/recurrentSet/constraintbuilder.cpp
        src/nonterm/recurrentSet/constraintbuilder.hpp
        src/nonterm/recurrentSet/strengthener.cpp
        src/nonterm/recurrentSet/strengthener.hpp
        src/nonterm/recurrentSet/types.hpp
        src/nonterm/recurrentSet/templatebuilder.cpp
        src/nonterm/recurrentSet/templatebuilder.hpp
        src/nonterm/recurrentSet/constraintsolver.cpp
        src/nonterm/recurrentSet/constraintsolver.hpp
        src/nonterm/recurrentSet/guardcontextbuilder.cpp
        src/nonterm/recurrentSet/guardcontextbuilder.hpp
        src/its/smt2Parser/sexpresso/sexpresso.cpp
        src/its/smt2Parser/sexpresso/sexpresso.hpp
        src/its/smt2Parser/parser.cpp
        src/its/smt2Parser/parser.hpp
        src/nonterm/nonterm.cpp
        src/nonterm/nonterm.hpp
        src/its/t2Parser/t2parser.cpp
        src/its/t2Parser/t2parser.hpp
        src/smt/smt.hpp
        src/smt/smt.cpp
        src/smt/smtcontext.hpp
        src/smt/exprtosmt.hpp
        src/smt/smttoexpr.hpp
        src/smt/z3/z3.cpp
        src/smt/z3/z3.hpp
        src/smt/z3/z3context.cpp
        src/smt/z3/z3context.hpp
        src/smt/yices/yices.cpp
        src/smt/yices/yices.hpp
        src/smt/yices/yicescontext.cpp
        src/smt/yices/yicescontext.hpp
        src/smt/smtfactory.cpp
        src/smt/smtfactory.hpp
        src/smt/model.cpp
        src/smt/model.hpp
        src/smt/combined_solver.hpp
        src/merging/merger.cpp
        src/merging/merger.hpp)

message(STATUS "Searching libraries")
find_library(Z3 z3)
message(STATUS "z3: ${Z3}")
find_library(YICES yices)
message(STATUS "yices: ${YICES}")
find_library(CVC4 cvc4)
message(STATUS "cvc4: ${CVC4}")
find_library(PURRS purrs)
message(STATUS "purrs: ${PURRS}")
find_library(GINAC ginac)
message(STATUS "ginac: ${GINAC}")
find_library(CLN cln)
message(STATUS "cln: ${CLN}")
find_library(GMP gmp)
message(STATUS "gmp: ${GMP}")
find_library(GOMP gomp HINTS /usr/lib/gcc/x86_64-linux-gnu/*/)
message(STATUS "gomp: ${GOMP}")
find_library(PTHREAD pthread)
message(STATUS "pthread: ${PTHREAD}")
find_library(NTL ntl)
message(STATUS "ntl: ${NTL}")

if (NOT YICES)
    message(STATUS "Yices not found.")
    return()
endif()

find_library(POLY poly)
message(STATUS "poly: ${POLY}")
find_library(CUDD cudd)
message(STATUS "cudd: ${CUDD}")
if (POLY AND CUDD)
    target_link_libraries(${EXECUTABLE} ${YICES} ${POLY} ${CUDD})
else()
    message(STATUS "Yices has missing dependencies.")
    return()
endif()

if (NOT Z3)
    message(STATUS "Z3 not found.")
    return()
endif()

target_link_libraries(${EXECUTABLE} ${Z3} ${PURRS} ${GINAC} ${NTL} ${CLN} ${GMP} ${GOMP} ${PTHREAD} ${LINKER_OPTIONS})
