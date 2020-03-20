#ifndef RESULT_HPP
#define RESULT_HPP

#include "../util/result.hpp"
#include "../util/proofoutput.hpp"
#include "../its/rule.hpp"

namespace Acceleration {

struct Result {
    std::vector<Rule> rules;
    Status status;
    ProofOutput proof;
};

}

#endif // RESULT_HPP
