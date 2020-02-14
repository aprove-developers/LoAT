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

#ifndef GLOBAL_H
#define GLOBAL_H

#include "config.hpp"
#include "util/proofoutput.hpp"

// global variables
namespace GlobalVariables {
    extern ProofOutput proofOutput;
}

// shorthands for global variables
#define proofout (GlobalVariables::proofOutput)

// dummy stream (to disable proof output)
#ifndef PROOF_OUTPUT_ENABLE
struct DummyStream {
    static std::ostream dummy;
};
#endif

#endif //GLOBAL_H
