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

#include <fstream>
#include <iostream>

#include "its/itsproblem.h"
#include "its/parser/itsparser.h"
#include "its/export.h"

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <input.koat> <outputfile>" << endl;
        return 1;
    }
    assert(argc == 3);

    string filename(argv[1]);
    ITSProblem res = parser::ITSParser::loadFromFile(filename);

    if (!res.isLinear()) {
        cout << "Error: T2 conversion only supported for linear (non-recursive) ITS problems" << endl;
        return 1;
    }

    string outname(argv[2]);
    ofstream outfile(outname);
    if (!outfile.is_open()) {
        cout << "Error: Unable to open output file: " << outname << endl;
        return 1;
    }

    LinearITSExport::printT2(res, outfile);
    outfile.close();
    return 0;
}
