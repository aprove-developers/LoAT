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
        src/expr/ginactoz3.cpp
        src/expr/ginactoz3.hpp
        src/expr/guardtoolbox.cpp
        src/expr/guardtoolbox.hpp
        src/expr/relation.cpp
        src/expr/relation.hpp
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
        src/util/timeout.cpp
        src/util/timeout.hpp
        src/z3/z3context.cpp
        src/z3/z3context.hpp
        src/z3/z3solver.hpp
        src/z3/z3solver.cpp
        src/z3/z3toolbox.cpp
        src/z3/z3toolbox.hpp
        src/config.cpp
        src/config.hpp
        src/global.cpp
        src/global.hpp
        src/main.cpp
        src/strengthening/constraintbuilder.cpp
        src/strengthening/constraintbuilder.hpp
        src/strengthening/strengthener.cpp
        src/strengthening/strengthener.hpp
        src/strengthening/types.hpp
        src/strengthening/modes.cpp
        src/strengthening/modes.hpp
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
        src/util/collections.hpp
        src/sexpresso/sexpresso.cpp
        src/sexpresso/sexpresso.hpp
        src/its/sexpressionparser/parser.cpp
        src/its/sexpressionparser/parser.hpp
        src/merging/rulemerger.cpp
        src/merging/rulemerger.hpp
        src/nonterm/nonterm.cpp
        src/nonterm/nonterm.hpp
        src/its/t2parser/t2parser.cpp
        src/its/t2parser/t2parser.hpp)

message(STATUS "Searching libraries")
find_library(Z3 z3)
message(STATUS "z3: ${Z3}")
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

# Z3 should be z3/bin/libz3.so, go to the parent directory twice to get Z3_HOME
get_filename_component(Z3_BIN ${Z3} DIRECTORY)
get_filename_component(Z3_HOME ${Z3_BIN} DIRECTORY)
message(STATUS "Setting Z3_HOME to ${Z3_HOME}")

target_include_directories(${EXECUTABLE} PUBLIC ${Z3_HOME}/include)

target_link_libraries(${EXECUTABLE} ${PURRS} ${GINAC} ${CLN} ${NTL} ${Z3} ${GMP} ${GOMP} ${PTHREAD} ${LINKER_OPTIONS})
