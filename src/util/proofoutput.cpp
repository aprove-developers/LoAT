#include "proofoutput.hpp"
#include "../its/export.hpp"

#include <iostream>
#include <string>

ProofOutput ProofOutput::Proof;
bool ProofOutput::enabled = true;

void ProofOutput::writeToFile(const std::string &file) const {
    if (enabled) {
        std::ofstream myfile;
        myfile.open(file);
        for (const auto &l: proof) {
            myfile << l.second << std::endl;
        }
        myfile.close();
    }
}

void ProofOutput::print() const {
    if (enabled) {
        for (const auto &l: proof) {
            if (Config::Output::Colors) {
                switch (l.first) {
                case None: std::cout << Config::Color::None;
                    break;
                case Result: std::cout << Config::Color::Result;
                    break;
                case Section: std::cout << Config::Color::Section;
                    break;
                case Headline: std::cout << Config::Color::Headline;
                    break;
                }
            }
            std::cout << l.second << std::endl;
        }
    }
}

void ProofOutput::append(const std::string &s) {
    append(Style::None, s);
}

void ProofOutput::append(const std::ostream &s) {
    std::stringstream str;
    str << s.rdbuf();
    append(str.str());
}

void ProofOutput::append(const Style &style, std::string s) {
    if (enabled) {
        std::vector<std::string> lines;
        boost::split(lines, s, boost::is_any_of("\n"));
        for (const std::string &l: lines) {
            proof.push_back({style, l});
        }
    }
}

void ProofOutput::newline() {
    append(std::stringstream());
}

void ProofOutput::headline(const std::string &s) {
    newline();
    append(Headline, s);
}

// print given string in headline style with spacing
void ProofOutput::headline(const std::ostream &s) {
    std::stringstream str;
    str << s.rdbuf();
    headline(str.str());
}

void ProofOutput::section(const std::string &s) {
    newline();
    append(Section, s);
}

void ProofOutput::section(const std::ostream &s) {
    std::stringstream str;
    str << s.rdbuf();
    section(str.str());
}

void ProofOutput::result(const std::string &s) {
    append(Result, s);
}

void ProofOutput::result(const std::ostream &s) {
    std::stringstream str;
    str << s.rdbuf();
    result(str.str());
}

bool ProofOutput::setEnabled(bool on) {
    bool res = enabled;
    enabled = on;
    return res;
}

void ProofOutput::concat(const ProofOutput &that) {
    if (enabled) {
        proof.insert(proof.end(), that.proof.begin(), that.proof.end());
    }
}

void ProofOutput::ruleTransformationProof(const Rule &oldRule, const std::string &transformation, const Rule &newRule, const ITSProblem &its) {
    section("Applied " + transformation);
    std::stringstream s;
    s << "Original rule:\n";
    ITSExport::printRule(oldRule, its, s);
    s << "\nNew rule:\n";
    ITSExport::printRule(newRule, its, s);
    append(s);
}

void ProofOutput::majorProofStep(const std::string &step, const ITSProblem &its) {
    headline(step);
    std::stringstream s;
    ITSExport::printForProof(its, s);
    append(s);
}

void ProofOutput::minorProofStep(const std::string &step, const ITSProblem &its) {
    section(step);
    std::stringstream s;
    ITSExport::printForProof(its, s);
    append(s);
}

void ProofOutput::deletionProof(const std::set<TransIdx> &rules) {
    if (!rules.empty()) {
        section("Applied deletion");
        std::stringstream s;
        s << "Removed the following rules:";
        for (TransIdx i: rules) {
            s << " " << i;
        }
        append(s);
    }
}

void ProofOutput::chainingProof(const Rule &fst, const Rule &snd, const Rule &newRule, const ITSProblem &its) {
    section("Applied chaining");
    std::stringstream s;
    s << "First rule:\n";
    ITSExport::printRule(fst, its, s);
    s << "\nSecond rule:\n";
    ITSExport::printRule(snd, its, s);
    s << "\nNew rule:\n";
    ITSExport::printRule(newRule, its, s);
    append(s);
}

void ProofOutput::storeSubProof(const ProofOutput &subProof, const std::string &technique) {
    const std::string &file = std::tmpnam(nullptr);
    subProof.writeToFile(file + ".txt");
    append("Sub-proof via " + technique + " written to file://" + file + ".txt");
}
