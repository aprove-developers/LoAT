#ifndef PROOFUTIL_H
#define PROOFUTIL_H

#include "proofoutput.hpp"
#include "../its/itsproblem.hpp"

ProofOutput ruleTransformationProof(const Rule &oldRule, const std::string &transformation, const Rule &newRule, const ITSProblem &its);

void majorProofStep(const std::string &step, const ITSProblem &its);

void minorProofStep(const std::string &step, const ITSProblem &its);

ProofOutput deletionProof(const std::set<TransIdx> &rules);

ProofOutput deletionProof(const Rule &rule, const ITSProblem &its);

ProofOutput storeSubProof(const ProofOutput &proof, const std::string &technique);

ProofOutput chainingProof(const Rule &fst, const Rule &snd, const Rule &newRule, const ITSProblem &its);

#endif // PROOFUTIL_H
