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
#include "expr/expression.h"
#include "expr/complexity.h"

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " [--simple] <arithmetic expression>" << endl << endl;
        cout << "Computes the complexity class of the given upper bound (from KoAT)" << endl;
        cout << "Note: This is only syntactic computation, so it might be wrong for complicated bounds!" << endl << endl;
        cout << "The --simple flag only affects the output (WST or LoAT style)." << endl;
        return 1;
    }

    bool simple = false;
    if (strcmp(argv[1],"--simple") == 0) {
        simple = true;
        assert(argc == 3);
    } else {
        assert(argc == 2);
    }

    GiNaC::symtab knownSyms;
    knownSyms["INF"] = Expression::InfSymbol;
    knownSyms["Inf"] = Expression::InfSymbol;
    GiNaC::parser p(knownSyms);
    Expression expr(p(argv[argc-1]));
    Complexity cpx = expr.getComplexity();

    if (cpx == Complexity::Unknown) {
        cout << "Error: Could not compute the complexity (bound is probably too complicated)" << endl;
    }

    if (simple) {
        //for own parsing
        cout << "Complexity: " << cpx << endl;
    } else {
        //for aprove benchmark system (similar to cpx.toWstString, but output an upper bound)
        if (cpx == Complexity::Nonterm) {
            cout << "NO" << endl;
        } else {
            cout << "WORST_CASE(?,";
            if (cpx == Complexity::Unknown || cpx == Complexity::Infty) cout << "?";
            else if (cpx >= Complexity::Exp) cout << "EXP";
            else if (cpx == Complexity::Const) cout << "O(1)";
            else cout << "Omega(n^" << cpx.getPolynomialDegree().toString() << ")";
            cout << ")" << endl;
        }
    }

    return 0;
}
