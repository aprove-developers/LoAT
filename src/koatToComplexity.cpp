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

#include <iostream>
#include "expression.h"

using namespace std;

int main(int argc, char *argv[]) {
    assert(argc >= 2);

    bool simple = false;
    if (strcmp(argv[1],"--simple") == 0) {
        simple = true;
        assert(argc == 3);
    } else {
        assert(argc == 2);
    }

    GiNaC::symtab knownSyms;
    knownSyms["INF"] = Expression::Infty;
    knownSyms["Inf"] = Expression::Infty;
    GiNaC::parser p(knownSyms);
    Expression expr(p(argv[argc-1]));
    Complexity cpx = expr.getComplexity();

    assert(cpx == Expression::ComplexNone || cpx >= 0);

    if (simple) {
        //for own parsing
        cout << "Complexity: ";
        if (cpx == Expression::ComplexInfty) cout << "INF" << endl;
        else if (cpx == Expression::ComplexExp) cout << "EXP" << endl;
        else cout << cpx << endl;
    } else {
        //for aprove benchmark system
        cout << "WORST_CASE(?,";
        if (cpx == Expression::ComplexNone || cpx == Expression::ComplexInfty) cout << "?";
        else if (cpx == Expression::ComplexExp) cout << "EXP";
        else if (cpx <= 0) cout << "O(1)";
        else cout << "O(n^" << cpx << ")";
        cout << ")" << endl;
    }

    return 0;
}
