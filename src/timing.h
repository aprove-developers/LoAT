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

#ifndef TIMING_H
#define TIMING_H

#include <ostream>


/**
 * Simple functions to keep track of how much time each component took.
 * This measures time for different actions (metering, chaining, preprocessing) as well as for the tools used (PURRS,Z3)
 */
namespace Timing
{
    enum TimingAction { Total=0, FarkasTotal, FarkasLogic, Contract, Branches, Selfloops, Infinity, Z3, Purrs, Preprocess, Unknown };

    void clear();
    void print(std::ostream &os);

    //adds the current time as starting point of the given action. Note that this must not be nested
    void start(TimingAction action);

    //uses the current time as an end point for the given action, adds the delta to the total elapsed time for this action
    void done(TimingAction action);

    class Scope {
    public:
        Scope(TimingAction ac) : action(ac) { Timing::start(action); }
        ~Scope() { Timing::done(action); }
    private:
        TimingAction action;
    };
}


#endif // TIMING_H
