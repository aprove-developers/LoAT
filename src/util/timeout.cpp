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

#include "timeout.hpp"
#include "../debug.hpp"

#include <algorithm>
#include <iostream>

using namespace std;

static bool timeout_enable = false;
static TimePoint timeout_start;
static TimePoint timeout_preprocess;
static TimePoint timeout_soft;
static TimePoint timeout_hard;

void Timeout::setTimeouts(int seconds) {
    assert(seconds == 0 || seconds >= 10);
    TimePoint now = chrono::steady_clock::now();
    timeout_start = now;

    if (seconds > 0) {
        timeout_preprocess = now + static_cast<std::chrono::seconds>((seconds < 30) ? 3 : 5);
        timeout_soft = now + static_cast<std::chrono::seconds>(seconds-((seconds < 30) ? 5 : 10));
        timeout_hard = now + static_cast<std::chrono::seconds>(seconds-2);
        timeout_enable = true;
    }
}

TimePoint Timeout::start() {
    return timeout_start;
}

bool Timeout::preprocessing() {
    if (!timeout_enable) return false;
    TimePoint now = chrono::steady_clock::now();
    return now >= timeout_preprocess;
}

bool Timeout::soft() {
    if (!timeout_enable) return false;
    TimePoint now = chrono::steady_clock::now();
    return now >= timeout_soft;
}

bool Timeout::hard() {
    if (!timeout_enable) return false;
    TimePoint now = chrono::steady_clock::now();
    return now >= timeout_hard;
}

TimePoint Timeout::create(int seconds) {
    TimePoint now = chrono::steady_clock::now();
    return now + static_cast<std::chrono::seconds>(seconds);
}

bool Timeout::over(const TimePoint &point) {
    TimePoint now = chrono::steady_clock::now();
    return now >= point;
}
