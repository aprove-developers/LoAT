cmake_minimum_required(VERSION 3.7)

project(LoAT)

set(CMAKE_CXX_STANDARD 11)

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

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O4")

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "Compiler flags:" ${CMAKE_CXX_COMPILE_FLAGS})
message(STATUS "Compiler cxx debug flags:" ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "Compiler cxx release flags:" ${CMAKE_CXX_FLAGS_RELEASE})
message(STATUS "Compiler cxx min size flags:" ${CMAKE_CXX_FLAGS_MINSIZEREL})
message(STATUS "Compiler cxx flags:" ${CMAKE_CXX_FLAGS})

add_executable(${EXECUTABLE}
        src/accelerate/meter/farkas.cpp
        src/accelerate/meter/farkas.hpp
        src/accelerate/meter/linearize.cpp
        src/accelerate/meter/linearize.hpp
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
        src/accelerate/backward.cpp
        src/accelerate/backward.hpp
        src/accelerate/forward.cpp
        src/accelerate/forward.hpp
        src/accelerate/accelerationproblem.hpp
        src/accelerate/boundextractor.hpp
        src/accelerate/boundextractor.cpp
        src/accelerate/vareliminator.cpp
        src/accelerate/vareliminator.hpp
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
        src/expr/guardtoolbox.cpp
        src/expr/guardtoolbox.hpp
        src/expr/relation.cpp
        src/expr/relation.hpp
        src/expr/boolexpr.cpp
        src/expr/boolexpr.hpp
        src/its/parser/itsparser.cpp
        src/its/parser/itsparser.hpp
        src/its/parser/term.cpp
        src/its/parser/term.hpp
        src/its/parser/termparser.cpp
        src/its/parser/termparser.hpp
        src/its/export.cpp
        src/its/export.hpp
        src/its/hypergraph.hpp
        src/its/itsproblem.cpp
        src/its/itsproblem.hpp
        src/its/rule.cpp
        src/its/rule.hpp
        src/its/types.cpp
        src/its/types.hpp
        src/its/variablemanager.cpp
        src/its/variablemanager.hpp
        src/util/exceptions.hpp
        src/util/option.hpp
        src/util/proofoutput.hpp
        src/util/proofoutput.cpp
        src/util/timeout.cpp
        src/util/timeout.hpp
        src/util/result.hpp
        src/config.cpp
        src/config.hpp
        src/main.cpp
        src/strengthening/constraintbuilder.cpp
        src/strengthening/constraintbuilder.hpp
        src/strengthening/strengthener.cpp
        src/strengthening/strengthener.hpp
        src/strengthening/types.hpp
        src/strengthening/templatebuilder.cpp
        src/strengthening/templatebuilder.hpp
        src/strengthening/templates.cpp
        src/strengthening/templates.hpp
        src/strengthening/constraintsolver.cpp
        src/strengthening/constraintsolver.hpp
        src/strengthening/rulecontextbuilder.cpp
        src/strengthening/rulecontextbuilder.hpp
        src/strengthening/guardcontextbuilder.cpp
        src/strengthening/guardcontextbuilder.hpp
        src/util/relevantvariables.cpp
        src/util/relevantvariables.hpp
        src/util/ginacutils.cpp
        src/util/ginacutils.hpp
        src/sexpresso/sexpresso.cpp
        src/sexpresso/sexpresso.hpp
        src/its/sexpressionparser/parser.cpp
        src/its/sexpressionparser/parser.hpp
        src/nonterm/nonterm.cpp
        src/nonterm/nonterm.hpp
        src/its/t2parser/t2parser.cpp
        src/its/t2parser/t2parser.hpp
        src/smt/smt.hpp
        src/smt/smt.cpp
        src/smt/smtcontext.hpp
        src/smt/ginactosmt.hpp
        src/smt/z3/z3.cpp
        src/smt/z3/z3.hpp
        src/smt/z3/z3context.cpp
        src/smt/z3/z3context.hpp
        src/smt/cvc4/cvc4.cpp
        src/smt/cvc4/cvc4.hpp
        src/smt/cvc4/cvc4context.cpp
        src/smt/cvc4/cvc4context.hpp
        src/smt/yices/yices.cpp
        src/smt/yices/yices.hpp
        src/smt/yices/yicescontext.cpp
        src/smt/yices/yicescontext.hpp
        src/smt/smtfactory.cpp
        src/smt/smtfactory.hpp)

message(STATUS "Searching libraries")
find_library(Z3 z3)
message(STATUS "z3: ${Z3}")
find_library(CVC4 cvc4)
message(STATUS "cvc4: ${CVC4}")
find_library(YICES yices)
message(STATUS "yices: ${YICES}")
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

if(Z3)
    add_compile_definitions(HAS_Z3)
    target_link_libraries(${EXECUTABLE} ${Z3})
    message(STATUS "has Z3")
endif()
if(CVC4)
    add_compile_definitions(HAS_CVC4)
    target_link_libraries(${EXECUTABLE} ${CVC4})
    message(STATUS "has CVC4")
endif()
if(YICES)
    add_compile_definitions(HAS_YICES)
    target_link_libraries(${EXECUTABLE} ${YICES})
    message(STATUS "has Yices")
endif()

if ((NOT YICES) AND (NOT Z3) AND (NOT CVC4))
    message(STATUS "no SMT solver found -- please install Yices, Z3, or CVC4")
    return()
endif()

target_link_libraries(${EXECUTABLE} ${PURRS} ${GINAC} ${NTL} ${CLN} ${GMP} ${GOMP} ${PTHREAD} ${LINKER_OPTIONS})
