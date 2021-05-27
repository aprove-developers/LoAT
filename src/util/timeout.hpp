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

#include <stdio.h>
#include <iostream>

#include <chrono>
typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;
typedef std::chrono::duration<std::chrono::seconds> Duration;


/**
 * Methods to allow aborting early for some given timeout value.
 * Currently, there are 3 timeouts derived from the given value:
 *  - preprocessing: this can take a long time, so the initial preprocessing is limited to a few seconds
 *  - soft: at this time, the normal logic is aborted to allow recovering at least a partial result
 *  - hard: at this time, the recovering logic is aborted, to finish in time
 *
 * Note that there is absolutely no guarantee that the program will stop in time,
 * but checks are done at reasonable places, so this should work in most cases.
 */
namespace Timeout {
    //calculates all relevant timeout points from this global timeout
    //call with 0 to disable timeouts
    void setTimeouts(unsigned int seconds);

    //return true if the timeout has already occurred
    bool hard();
    bool soft();
    bool enabled();

    std::chrono::seconds remainingSoft();
    std::chrono::seconds remainingHard();
}
