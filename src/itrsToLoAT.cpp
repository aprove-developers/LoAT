#include <iostream>
#include <fstream>

#include "expression.h"
#include "itrs/itrs.h"
#include "itrs/term.h"

void writeRules(const ITRS &itrs,
                const std::vector<ITRSRule> &rules,
                std::ostream &os) {
    std::vector<FunctionSymbolIndex> allFunSyms;
    std::set<FunctionSymbolIndex> allFunSymsSet;
    for (const ITRSRule &rule : rules) {
        rule.lhs.collectFunctionSymbols(allFunSyms);
        rule.lhs.collectFunctionSymbols(allFunSymsSet);
    }
    assert(!allFunSyms.empty());

    os << "(GOAL COMPLEXITY)" << std::endl;
    if (itrs.startFunctionSymbolWasDeclared()) {
        os << "(STARTTERM (FUNCTIONSYMBOLS ";
        if (allFunSymsSet.count(itrs.getStartFunctionSymbol()) == 1) {
           os << itrs.getFunctionSymbolName(itrs.getStartFunctionSymbol());

        } else {
            os << itrs.getFunctionSymbolName(allFunSyms.front());
        }

        os << "))" << std::endl;
    }

    os << "(VAR";
    for (const std::string &var : itrs.getVariables()) {
        os << " " << var;
    }
    os << ")" << std::endl;

    os << "(RULES" << std::endl;
    for (const ITRSRule &rule : rules) {
        os << rule.lhs;

        if (rule.cost.info(TT::InfoFlag::Number)
            && rule.cost.toGiNaC().is_equal(GiNaC::numeric(1))) {
            os << " -> ";

        } else {
            os << " -{" << rule.cost << "}> ";
        }

        os << rule.rhs;

        if (!rule.guard.empty()) {
            os << " :|: ";
        }

        for (int i = 0; i < rule.guard.size(); ++i) {
            if (i > 0) {
                os << " && ";
            }

            os << rule.guard[i];
        }

        os << std::endl;
    }
    os << ")";

    os << std::endl;
}

std::set<FunctionSymbolIndex> findFunctionSymbolsToAbstract(const ITRS &itrs) {
    std::set<FunctionSymbolIndex> toAbstract;
    // all undefined function symbols
    for (int i = 0; i < itrs.getFunctionSymbolCount(); ++i) {
        toAbstract.insert(i);
    }
    for (const ITRSRule &rule : itrs.getITRSRules()) {
        if (rule.lhs.hasNoFunctionSymbols()) {
            continue;
        }

        toAbstract.erase(rule.lhs.getFunctionSymbolsAsVector().front());
    }

    // all function symbols that appear nested in a lhs
    for (const ITRSRule &rule : itrs.getITRSRules()) {
        if (rule.lhs.hasNoFunctionSymbols()) {
            continue;
        }

        std::vector<FunctionSymbolIndex> funSyms = std::move(rule.lhs.getFunctionSymbolsAsVector());
        for (int i = 1; i < funSyms.size(); ++i) {
            toAbstract.insert(funSyms[i]);
        }
    }

    return toAbstract;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        std::cout << "Usage: " << argv[0] << " in.itrs [out.koat]" << std::endl;
        return 1;
    }

    ITRS itrs = ITRS::loadFromFile(argv[1]);
    itrs.print(std::cout);

    std::set<FunctionSymbolIndex> toAbstract = std::move(findFunctionSymbolsToAbstract(itrs));

    std::vector<ITRSRule> modifiedRules;
    for (const ITRSRule &rule : itrs.getITRSRules()) {
        ITRSRule res;

        if (!rule.lhs.info(TT::InfoFlag::FunctionSymbol)) {
            // Can't do anything, skip
            // (Shouldn't occur)
            continue;
        }

        std::vector<FunctionSymbolIndex> funSyms = rule.lhs.getFunctionSymbolsAsVector();
        FunctionSymbolIndex definingFunSym = funSyms.front();

        // make sure all variables are non-negative
        ExprSymbolSet variables;
        for (ExprSymbol var : rule.lhs.getVariables()) {
            variables.insert(var);
        }
        for (ExprSymbol var : rule.rhs.getVariables()) {
            variables.insert(var);
        }
        for (ExprSymbol var : variables) {
            res.guard.push_back(TT::Expression(var >= 0));
        }

        // term size abstraction of the lhs
        res.lhs = rule.lhs.abstractSize(toAbstract);
        if (!res.lhs.info(TT::InfoFlag::FunctionSymbol)) {
            // the function symbol occured nested in a lhs
            continue;
        }
        assert(res.lhs.isSimple());

        // move the expressions from the lhs to the guard
        std::vector<TT::Expression> argVariables;
        for (int i = 0; i < res.lhs.nops(); ++i) {
            const TT::Expression &arg = res.lhs.op(i);

            if (arg.info(TT::InfoFlag::Variable)) {
                ExprSymbol ginacSymbol = GiNaC::ex_to<GiNaC::symbol>(res.lhs.op(i).toGiNaC());
                argVariables.push_back(TT::Expression(ginacSymbol));

            } else {
                VariableIndex newVar = itrs.addFreshVariable("x");
                ExprSymbol ginacSymbol = itrs.getGinacSymbol(newVar);
                argVariables.push_back(TT::Expression(ginacSymbol));

                res.guard.push_back((TT::Expression(ginacSymbol) == arg).ginacify());

                // Make sure the abstracted term is positive
                res.guard.push_back((TT::Expression(ginacSymbol) > 0).ginacify());
            }
        }

        // build the new lhs
        res.lhs = TT::Expression(definingFunSym,
                                 itrs.getFunctionSymbolName(definingFunSym),
                                 argVariables);

        // abstraction of the rhs
        res.rhs = rule.rhs.abstractSize(toAbstract).ginacify();

        // copy the cost
        if (!rule.cost.hasNoFunctionSymbols()) {
            continue;
        }
        res.cost = rule.cost.ginacify();

        // add the remaining guard
        bool skipRule = false;
        for (const TT::Expression &ex : rule.guard) {
            res.guard.push_back(ex.ginacify());

            if (!ex.hasNoFunctionSymbols()) {
                skipRule = true;
            }
        }
        if (skipRule) {
            continue;
        }

        modifiedRules.push_back(std::move(res));
    }

    writeRules(itrs, modifiedRules, std::cout);
    if (argc == 3) {
        std::ofstream out(argv[2]);
        writeRules(itrs, modifiedRules, out);
    }

    return 0;
}
