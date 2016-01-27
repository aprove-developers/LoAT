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

#include "farkas.h"
#include "itrs.h"
#include "flowgraph.h"

#include <iostream>

using namespace std;

int main(int argc, char* argv[]) {
    assert(argc == 2);

    //create list of allowed variable names
    string allowedVars = "abcdefghABCDEFGHxyzXYZrstRST";
    vector<string> vars;
    for (char c : allowedVars) vars.push_back(string(1,c));

    //create the ITRSProblem from a single rule
    ITRSProblem itrs = ITRSProblem::dummyITRSforTesting(vars,{argv[1]});

    //we require this to be a selfloop
    assert(itrs.getTermCount() == 1);

    //transform to flow graph, remove selfloops to apply farkas
    FlowGraph g(itrs);
    return (int)!g.removeSelfloops();
}
