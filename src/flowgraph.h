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

#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

#include "global.h"

#include "graph/graph.h"
#include "itrs.h"
#include "expr/expression.h"

/**
 * Represents one transition in the graph with the given target, guards and updates
 */
struct Transition {
    Transition() : cost(1) {}
    GuardList guard;
    UpdateMap update;
    Expression cost;
};

std::ostream& operator<<(std::ostream &, const Transition &);

#endif // FLOWGRAPH_H
