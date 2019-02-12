//
// Created by ffrohn on 2/8/19.
//

#ifndef LOAT_STRENGTHENING_H
#define LOAT_STRENGTHENING_H

#include <boost/optional.hpp>
#include <its/rule.h>

namespace Strengthening {

    std::vector<Rule> apply(const std::vector<Rule> &predecessors, const Rule &r, VariableManager &varMan);

}

#endif //LOAT_STRENGTHENING_H
