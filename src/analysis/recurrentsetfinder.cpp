#include "recurrentsetfinder.hpp"
#include "../smt/yices/yices.hpp"
#include "../util/proof.hpp"
#include "../accelerate/accelerationCalculus/accelerationproblem.hpp"

void RecurrentSetFinder::run(ITSProblem &its) {
    Yices::init();
    for (auto loc: its.getLocations()) {
        for (auto idx: its.getSimpleLoopsAt(loc)) {
            const auto &rule = its.getRule(idx).toLinear();
            AccelerationProblem ap = AccelerationProblem::initForRecurrentSet(rule, its);
            auto accelRes = ap.computeRes();
            if (!accelRes.empty()) {
                Proof proof;
                std::stringstream headline;
                headline << "Searching Recurrent Set for Transition #" << idx;
                proof.majorProofStep(headline.str(), its);
                proof.concat(ap.getProof());
                proof.newline();
                proof.result("Found Recurrent Set!");
                std::cout << "NO" << std::endl;
                proof.print();
                break;
           }
        }
    }
    Yices::exit();
}
