#ifndef ITSPARSER_H
#define ITSPARSER_H

#include <string>
#include <vector>

#include <boost/optional.hpp>

#include "term.h"
#include "termparser.h"
#include "its/itsproblem.h"

namespace parser {

class ITSParser {
public:
    struct Settings {
        // Whether to allow division in arithmetic expressions.
        // Note: This is only sound if the result is guaranteed to be an integer value!
        bool allowDivison = false;

        // Whether to add the term "cost >= 0" to the guard.
        // This ensures that transitions can only be taken when the cost evaluates to a non-negative value.
        // Note: The current implementation relies on this property! Disabling may be unsound.
        bool ensureNonnegativeCosts = true;
    };

    /**
     * Tries to load the given file and convert it into an ITSProblem
     * @param path The file to load
     * @param cfg Settings to control certain restrictions during parsing
     * @return The resulting ITSProblem (a FileError is thrown if parsing fails)
     */
    static ITSProblem loadFromFile(const std::string &path, Settings cfg);
    EXCEPTION(FileError,CustomException);

private:
    // Intermediate rule representation during parsing
    struct ParsedRule {
        TermPtr lhs;
        std::vector<TermPtr> rhss;
        boost::optional<TermPtr> cost;
        std::vector<Relation> guard;
    };

    struct LocationData {
        LocationIdx index;
        int arity;
        std::vector<VariableIdx> lhsVars;
    };


    explicit ITSParser(Settings cfg) : settings(cfg) {}
    ITSProblem load(const std::string &path);

    // High-level parsing steps
    void parseFile(std::ifstream &file);
    void convertRules();

    // Step 1: Parsing into ParsedRule
    ParsedRule parseRule(const std::string &line) const;
    TermPtr parseTerm(const std::string &s) const;
    boost::optional<TermPtr> parseCost(const std::string &cost) const;
    TermPtr parseLeftHandSide(const std::string &rhs) const;
    std::vector<TermPtr> parseRightHandSide(const std::string &rhs) const;
    std::vector<Relation> parseGuard(const std::string &guard) const;

    // Step 2: Converting ParsedRules to ITSProblem
    void addAndCheckLocationData(TermPtr term, bool lhs);
    const LocationData& getLocationData(TermPtr term) const;
    GiNaC::exmap computeSubstitutionToUnifyLhs(const ParsedRule &rule);
    void replaceUnboundedByTemporaryVariables(NonlinearRule &rule, const LocationData &lhsData);
    void stripTrivialUpdates(UpdateMap &update) const;
    void addParsedRule(const ParsedRule &rule);

    // Helpers
    static std::string escapeVariableName(const std::string &name);
    static std::set<VariableIdx> getVariables(const ParsedRule &rule);
    static void applySubstitution(NonlinearRule &rule, const GiNaC::exmap &subs);

private:
    // Instance of TermParser, created after knownVariables has been set
    mutable std::unique_ptr<TermParser> termParser = nullptr;

    // Step 1: Parsing into ParsedRule
    std::string initialLocation;
    std::map<std::string, VariableIdx> knownVariables;
    std::vector<ParsedRule> parsedRules;

    // Step 2: Converting ParsedRules to ITSProblem
    std::map<std::string, LocationData> knownLocations;
    ITSProblem itsProblem;

    // Settings for the parser
    Settings settings;
};

}

#endif //ITSPARSER_H
