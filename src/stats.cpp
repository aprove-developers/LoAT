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

#include "stats.h"
#include "global.h"

#include <map>
#include <vector>

using namespace std;

typedef map<Stats::StatAction,int> StatData;

static int step = 0;
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
        os << name << ": " << val << endl;
    };

    int unsat = 0;
    int fail = 0;

    os << " ======== STATS =========" << endl;
    for (int i=0; i <= step; ++i) {
        os << " ---- " << names[i] << " ----" << endl;
        printVal(data[i][ContractLinear],"Contract[Linear]");
        printVal(data[i][ContractBranch],"Contract[Branched]");
        printVal(data[i][ContractUnsat], "Contract[Unsat]");
        printVal(data[i][SelfloopRanked],"Loop[Ranked]");
        printVal(data[i][SelfloopNoRank],"Loop[NoRank]");
        printVal(data[i][SelfloopNoUpdate],"Loop[NoUpdate]");
        printVal(data[i][SelfloopInfinite],"Loop[Infinite]");
        printVal(data[i][PruneRemove], "Pruned[Removed]");

        unsat += data[i][ContractUnsat];
        fail += data[i][SelfloopNoRank] + data[i][SelfloopNoUpdate];
    }

    if (unsat > 0) os << "NOTE: Contract UNSATs: " << unsat << endl;
    if (fail > 0) os << "CRITICAL: Loop failures: " << fail << endl;
    os << " ======== STATS =========" << endl;
}
