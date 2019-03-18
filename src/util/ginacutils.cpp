//
// Created by ffrohn on 3/14/19.
//

#include "ginacutils.h"
#include <ginac/ginac.h>

namespace util {

    typedef GiNaCUtils Self;

    GiNaC::exmap Self::compose(const GiNaC::exmap &fst, const GiNaC::exmap &snd) {
        GiNaC::exmap res;
        for (const auto &p: fst) {
            res[p.first] = p.second.subs(snd);
        }
        for (const auto &p: snd) {
            if (res.count(p.first) == 0) {
                res[p.first] = p.second;
            }
        }
        return res;
    }

    GiNaC::exmap Self::concat(const GiNaC::exmap &fst, const GiNaC::exmap &snd) {
        GiNaC::exmap res;
        for (const auto &p: fst) {
            res[p.first] = p.second.subs(snd);
        }
        return res;
    }

}
