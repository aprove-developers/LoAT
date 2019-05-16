/*  This file is part of LoAT.
 *  Copyright (c) 2019 Florian Frohn
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

#ifndef LOAT_UTIL_COLLECTIONS_H
#define LOAT_UTIL_COLLECTIONS_H

#include <set>

namespace util {

    class Collections {

    public:
        template<typename Elem, typename Comp> static void removeAll(std::set<Elem, Comp> &a, const std::set<Elem, Comp> &b) {
            auto it = a.begin();
            while (it != a.end()) {
                if (b.find(*it) == b.end()) {
                    it++;
                } else {
                    it = a.erase(it);
                }
            }
        }

    };

}

#endif //LOAT_UTIL_COLLECTIONS_H
