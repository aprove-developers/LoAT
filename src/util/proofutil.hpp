#ifndef PROOFUTIL_H
#define PROOFUTIL_H

#include "proofoutput.hpp"
#include "../its/itsproblem.hpp"

ProofOutput ruleTransformationProof(const Rule &oldRule, const std::string &transformation, const Rule &newRule, const ITSProblem &its);

ProofOutput majorProofStep(const std::string &step, const ITSProblem &its);

ProofOutput deletionProof(const std::set<TransIdx> &rules);

#endif // PROOFUTIL_H
