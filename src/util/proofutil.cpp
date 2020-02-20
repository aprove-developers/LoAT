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

void majorProofStep(const std::string &step, const ITSProblem &its) {
    proofout.headline(step);
    std::stringstream s;
    ITSExport::printForProof(its, s);
    proofout.append(s);
}

void minorProofStep(const std::string &step, const ITSProblem &its) {
    proofout.section(step);
    std::stringstream s;
    ITSExport::printForProof(its, s);
    proofout.append(s);
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

ProofOutput deletionProof(const Rule &rule, const ITSProblem &its) {
    ProofOutput proof;
    proof.section("Applied deletion");
    std::stringstream s;
    s << "removed the following rule:\n";
    ITSExport::printRule(rule, its, s);
    proof.append(s);
    return proof;
}

ProofOutput chainingProof(const Rule &fst, const Rule &snd, const Rule &newRule, const ITSProblem &its) {
    ProofOutput proof;
    proof.section("Applied chaining");
    std::stringstream s;
    s << "First rule:\n";
    ITSExport::printRule(fst, its, s);
    s << "\nSecond rule:\n";
    ITSExport::printRule(snd, its, s);
    s << "\nNew rule:\n";
    ITSExport::printRule(newRule, its, s);
    proof.append(s);
    return proof;
}

ProofOutput storeSubProof(const ProofOutput &proof, const std::string &technique) {
    const boost::filesystem::path &file = getProofFile();
    proof.writeToFile(file);
    ProofOutput ret;
    ret.append("Sub-proof via " + technique + " written to file://" + file.native());
    return ret;
}
