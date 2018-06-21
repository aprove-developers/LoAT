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

#include "timing.h"
#include "debug.h"

#include <chrono>
#include <map>
#include <iomanip>

using namespace std;

typedef chrono::time_point<chrono::high_resolution_clock> timepoint;

static map<Timing::TimingAction,timepoint> TimingLast;
static map<Timing::TimingAction,std::chrono::milliseconds> TimingSum;

void Timing::clear() {
    TimingLast.clear();
    TimingSum.clear();
}

void Timing::start(TimingAction action) {
    timepoint now = chrono::high_resolution_clock::now();
    assert(TimingLast.count(action) == 0);
    TimingLast.emplace(action,now);
}

void Timing::done(TimingAction action) {
    timepoint now = chrono::high_resolution_clock::now();
    auto start = TimingLast.find(action);
    assert(start != TimingLast.end());
    TimingSum[action] += chrono::duration_cast<chrono::milliseconds>(now - start->second);
    TimingLast.erase(start);
}

void Timing::print(ostream &s) {
    auto printLine = [&](TimingAction ac, string name) {
        s << setw(10);
        auto it = TimingSum.find(ac);
        if (it == TimingSum.end()) s << "--"; else s << it->second.count();
        s << " | ";
        if (TimingLast.find(ac) != TimingLast.end()) s << "[active] ";
        s << name << endl;
    };

    s << " ========== TIMING ==========" << endl;
    s << setw(10) << "Time [ms]" << " | " << "Description" << endl;
    printLine(Total,"Total");
    s << " ----------------------------" << endl;
    printLine(Z3,"Z3 (total time in add/check)");
    printLine(Purrs,"PURRS (total time)");
    s << " ----------------------------" << endl;
    printLine(Preprocess,"Pre-processing");
    printLine(Prune,"Pruning (parallel rules)");
    printLine(Chain,"Chaining");
    printLine(Accelerate,"Acceleration (meter + backward)");
    printLine(Asymptotic,"Asymptotic Computation (only final computation)");
    s << " ----------------------------" << endl;
    printLine(Meter, "Metering (without heuristics)");
    printLine(BackwardAccel,"Backward Accel");
    if (TimingSum.count(Timing::Other) > 0) printLine(Other,"Other");
    s << " ========== TIMING ==========" << endl;
}
