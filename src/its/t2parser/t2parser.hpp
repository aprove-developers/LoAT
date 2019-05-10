#ifndef LOAT_T2_PARSER_H
#define LOAT_T2_PARSER_H


#include "../../its/itsproblem.hpp"

namespace t2parser {

    class T2Parser {

    public:
        static ITSProblem loadFromFile(const std::string &filename);

    private:
        void run(const std::string &filename);

        void parseTransition(LocationIdx start, std::ifstream &ifs);

        LocationIdx getLoc(const std::string &str);

        VariableIdx addVar(const std::string &str);

        Expression parseConstraint(const std::string &str);

        Expression parseExpression(std::string str);

        std::string trim(std::string toTrim, std::string prefix, std::string suffix);

        std::string removeComment(const std::string &string);

        ITSProblem res;

        const std::string START = "START:";
        const std::string FROM = "FROM:";
        const std::string TO = "TO:";
        const std::string ASSUME = "assume(";
        const std::string NONDET = "nondet()";
        const std::string ASSIGN = ":=";
        std::map<std::string, LocationIdx> locs;
        std::map<std::string, VariableIdx> vars;
        std::map<std::string, GiNaC::ex> symtab;

    };

}

#endif //LOAT_T2_PARSER_H
