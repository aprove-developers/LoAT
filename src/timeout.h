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
typedef std::chrono::time_point<std::chrono::steady_clock> timeoutpoint;

namespace Timeout {
    //calculates all relevant timeout points from this global timeout
    void setTimeouts(int seconds);

    //return true if the timeout has already occurred
    bool preprocessing();
    bool soft();
    bool hard();

    //custom timeouts
    timeoutpoint create(int seconds);
    bool over(const timeoutpoint &point);
}
