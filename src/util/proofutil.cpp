#include "proofutil.hpp"
#include "../its/export.hpp"

ProofOutput ruleTransformationProof(const Rule &oldRule, const std::string &transformation, const Rule &newRule, const ITSProblem &its) {
    ProofOutput proof;
    proof.section("Applied " + transformation);
    std::stringstream s;
    s << "Original rule:\n";
    ITSExport::printRule(oldRule, its, s);
    s << "\nNew rule:\n";
    ITSExport::printRule(newRule, its, s);
    proof.append(s);
    return proof;
}

ProofOutput majorProofStep(const std::string &step, const ITSProblem &its) {
    ProofOutput proof;
    proof.headline(step);
    std::stringstream s;
    ITSExport::printForProof(its, s);
    proof.append(s);
    return proof;
}

ProofOutput deletionProof(const std::set<TransIdx> &rules) {
    ProofOutput proof;
    if (!rules.empty()) {
        proof.section("Applied deletion");
        std::stringstream s;
        s << "removed the following rules:";
        for (TransIdx i: rules) {
            s << " " << i;
        }
        proof.append(s);
    }
    return proof;
}
