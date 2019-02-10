//
// Created by ffrohn on 2/8/19.
//

#ifndef LOAT_STRENGTHENING_H
#define LOAT_STRENGTHENING_H

#include <boost/optional.hpp>
#include <its/rule.h>

namespace Strengthening {

    boost::optional<Rule> apply(const Rule &r, VariableManager &varMan);

}

#endif //LOAT_STRENGTHENING_H
