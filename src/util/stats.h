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

#ifndef STATS_H
#define STATS_H

#include <string>
#include <ostream>


/**
 * Simple functions to keep track of some statistics (i.e. how many loops were metered, how many chaining steps performed etc.).
 */
namespace Stats
{
    enum StatAction {
        ChainSuccess=0, ChainFail,
        PruneRemove,
        MeterSuccess, MeterUnsat, MeterTooComplicated, MeterCannotIterate, MeterNonterm,
        BackwardSuccess, BackwardNoInverseUpdate, BackwardNonMonotonic, BackwardCannotIterate
    };

    void clear();
    void add(StatAction action);
    void addStep(const std::string &name);
    void print(std::ostream &os, bool printZero = false);
}


#endif // STATS_H
