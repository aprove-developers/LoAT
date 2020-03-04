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

#include "ginacutils.hpp"
#include <ginac/ginac.h>

namespace util {

    typedef GiNaCUtils Self;

    ExprMap Self::compose(const ExprMap &fst, const ExprMap &snd) {
        ExprMap res;
        for (const auto &p: fst) {
            res[p.first] = p.second.subs(snd);
        }
        for (const auto &p: snd) {
            if (!res.contains(p.first)) {
                res[p.first] = p.second;
            }
        }
        return res;
    }

    ExprMap Self::concat(const ExprMap &fst, const ExprMap &snd) {
        ExprMap res;
        for (const auto &p: fst) {
            res[p.first] = p.second.subs(snd);
        }
        return res;
    }

}
