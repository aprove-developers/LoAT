//
// Created by ffrohn on 3/18/19.
//

#ifndef LOAT_SEXPRESSION_PARSER_H
#define LOAT_SEXPRESSION_PARSER_H


#include "../../its/itsproblem.hpp"
#include "../../sexpresso/sexpresso.hpp"

namespace sexpressionparser {

    class Parser {

    public:
        static ITSProblem loadFromFile(const std::string &filename);

    private:
        void run(const std::string &filename);

        void parseCond(sexpresso::Sexp &sexp, GuardList &guard);

        GiNaC::ex parseConstraint(sexpresso::Sexp &sexp, bool negate);

        GiNaC::ex parseExpression(sexpresso::Sexp &sexp);

        std::vector<std::string> preVars;
        std::vector<std::string> postVars;
        std::map<std::string, LocationIdx > locations;
        std::map<std::string, VariableIdx> vars;
        ITSProblem res;

    };

}

#endif //LOAT_SEXPRESSION_PARSER_H
