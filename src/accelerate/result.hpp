#ifndef RESULT_HPP
#define RESULT_HPP

#include "../util/status.hpp"
#include "../util/proof.hpp"
#include "../its/rule.hpp"

namespace Acceleration {

struct Result {
    std::vector<Rule> rules;
    Status status;
    Proof proof;
};

}

#endif // RESULT_HPP
