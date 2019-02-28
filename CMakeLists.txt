cmake_minimum_required(VERSION 3.7)

project(LoAT)

set(CMAKE_CXX_STANDARD 11)

# non-static build by default
option(STATIC "static" OFF)

if(${STATIC})
    message(STATUS "Configuring static build")
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    set(BUILD_SHARED_LIBS OFF)
    set(LINKER_OPTIONS "-static")
    set(EXECUTABLE loat-static)
else()
    message(STATUS "Configuring non-static build")
    set(LINKER_OPTIONS "")
    set(EXECUTABLE loat)
endif()

include_directories(src)
include_directories(src/accelerate)
include_directories(src/accelerate/meter)
include_directories(src/accelerate/recurrence)
include_directories(src/analysis)
include_directories(src/asymptotic)
include_directories(src/expr)
include_directories(src/its)
include_directories(src/its/parser)
include_directories(src/util)
include_directories(src/z3)

add_executable(${EXECUTABLE}
        src/accelerate/meter/farkas.cpp
        src/accelerate/meter/farkas.h
        src/accelerate/meter/linearize.cpp
        src/accelerate/meter/linearize.h
        src/accelerate/meter/metering.cpp
        src/accelerate/meter/metering.h
        src/accelerate/meter/metertools.cpp
        src/accelerate/meter/metertools.h
        src/accelerate/recurrence/dependencyorder.cpp
        src/accelerate/recurrence/dependencyorder.h
        src/accelerate/recurrence/recurrence.cpp
        src/accelerate/recurrence/recurrence.h
        src/accelerate/accelerator.cpp
        src/accelerate/accelerator.h
        src/accelerate/backward.cpp
        src/accelerate/backward.h
        src/accelerate/forward.cpp
        src/accelerate/forward.h
        src/analysis/analysis.cpp
        src/analysis/analysis.h
        src/analysis/chain.cpp
        src/analysis/chain.h
        src/analysis/chainstrategy.cpp
        src/analysis/chainstrategy.h
        src/analysis/preprocess.cpp
        src/analysis/preprocess.h
        src/analysis/prune.cpp
        src/analysis/prune.h
        src/asymptotic/asymptoticbound.cpp
        src/asymptotic/asymptoticbound.h
        src/asymptotic/inftyexpression.cpp
        src/asymptotic/inftyexpression.h
        src/asymptotic/limitproblem.cpp
        src/asymptotic/limitproblem.h
        src/asymptotic/limitsmt.cpp
        src/asymptotic/limitsmt.h
        src/asymptotic/limitvector.cpp
        src/asymptotic/limitvector.h
        src/expr/complexity.cpp
        src/expr/complexity.h
        src/expr/expression.cpp
        src/expr/expression.h
        src/expr/ginactoz3.cpp
        src/expr/ginactoz3.h
        src/expr/guardtoolbox.cpp
        src/expr/guardtoolbox.h
        src/expr/relation.cpp
        src/expr/relation.h
        src/its/parser/itsparser.cpp
        src/its/parser/itsparser.h
        src/its/parser/term.cpp
        src/its/parser/term.h
        src/its/parser/termparser.cpp
        src/its/parser/termparser.h
        src/its/export.cpp
        src/its/export.h
        src/its/hypergraph.h
        src/its/itsproblem.cpp
        src/its/itsproblem.h
        src/its/rule.cpp
        src/its/rule.h
        src/its/types.cpp
        src/its/types.h
        src/its/variablemanager.cpp
        src/its/variablemanager.h
        src/util/exceptions.h
        src/util/option.h
        src/util/proofoutput.h
        src/util/stats.cpp
        src/util/stats.h
        src/util/timeout.cpp
        src/util/timeout.h
        src/util/timing.cpp
        src/util/timing.h
        src/z3/z3context.cpp
        src/z3/z3context.h
        src/z3/z3solver.h
        src/z3/z3solver.cpp
        src/z3/z3toolbox.cpp
        src/z3/z3toolbox.h
        src/config.cpp
        src/config.h
        src/debug.h
        src/global.cpp
        src/global.h
        src/main.cpp src/strengthening/constraintbuilder.cpp
        src/strengthening/constraintbuilder.h
        src/strengthening/strengthener.cpp
        src/strengthening/strengthener.h
        src/strengthening/types.h
        src/strengthening/modes.cpp
        src/strengthening/modes.h
        src/strengthening/templatebuilder.cpp
        src/strengthening/templatebuilder.h
        src/strengthening/templates.cpp
        src/strengthening/templates.h
        src/strengthening/constraintsolver.cpp
        src/strengthening/constraintsolver.h
        src/strengthening/rulecontextbuilder.cpp
        src/strengthening/rulecontextbuilder.h
        src/strengthening/guardcontextbuilder.cpp
        src/strengthening/guardcontextbuilder.h)

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
find_library(GOMP gomp HINTS /usr/lib/gcc/x86_64-linux-gnu/6/)
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
