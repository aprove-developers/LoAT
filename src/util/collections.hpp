//
// Created by ffrohn on 3/15/19.
//

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
