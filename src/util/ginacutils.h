//
// Created by ffrohn on 3/14/19.
//

#ifndef LOAT_UTIL_GINACUTILS_H
#define LOAT_UTIL_GINACUTILS_H

#include <ginac/basic.h>
#include <expr/expression.h>

namespace util {

    class GiNaCUtils {

    public:
        static GiNaC::exmap compose(const GiNaC::exmap &fst, const GiNaC::exmap &snd);
        static GiNaC::exmap concat(const GiNaC::exmap &fst, const GiNaC::exmap &snd);

    };

}

#endif //LOAT_UTIL_GINACUTILS_H
