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

#include "stats.hpp"
#include "../global.hpp"

#include <map>
#include <vector>

using namespace std;

typedef map<Stats::StatAction,int> StatData;

static unsigned int step = 0;
static map<int,StatData> data;
static vector<string> names = { "Initial" };


void Stats::clear() {
    step = 0;
    data.clear();
    names = { "Initial" };
}

void Stats::add(StatAction action) {
    data[step][action]++;
}

void Stats::addStep(const string &name) {
    step++;
    assert(names.size() == step);
    names.push_back(name);
}

void Stats::print(ostream &os, bool printZero) {
    auto printVal = [&](int val, string name) {
        if (val == 0 && !printZero) return;
        os << left << setw(20) << (name + ": ") << val << right << endl;
    };

    os << " ======== STATS =========" << endl;
    for (unsigned int i=0; i <= step; ++i) {
        os << " ---- " << names[i] << " ----" << endl;
        printVal(data[i][ChainSuccess],"Chain[success]");
        printVal(data[i][ChainFail],"Chain[fail]");
        printVal(data[i][PruneRemove], "Pruned");
        printVal(data[i][MeterSuccess],"Meter[success]");
        printVal(data[i][MeterUnsat],"Meter[unsat]");
        printVal(data[i][MeterTooComplicated],"Meter[too complicated]");
        printVal(data[i][MeterCannotIterate],"Meter[cannot iterate]");
        printVal(data[i][MeterNonterm], "Meter[nonterm]");
        printVal(data[i][BackwardSuccess], "Backward[success]");
        printVal(data[i][BackwardNonMonotonic], "Backward[not monotonic]");
        printVal(data[i][BackwardCannotIterate], "Backward[cannot iterate]");
    }
    os << " ======== STATS =========" << endl;
}
