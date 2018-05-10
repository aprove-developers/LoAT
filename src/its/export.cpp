#include "export.h"

using namespace std;


#ifdef COLORS_ITS_EXPORT
    #define COLOR_LOCATION "\033[0;34m" // bold blue
    #define COLOR_UPDATE "\033[0;36m" // cyan
    #define COLOR_GUARD "\033[0;32m" // green
    #define COLOR_COST "\033[0;33m" // yellow
    #define COLOR_NONE "\033[0m" // reset color to default
#else
    #define COLOR_LOCATION ""
    #define COLOR_UPDATE ""
    #define COLOR_GUARD ""
    #define COLOR_COST ""
    #define COLOR_NONE ""
#endif


// TODO: Looks suspiciuos, might produce a function symbol with different number of arguments
// TODO: E.g., for f(x,y) -> g(x), the variable y is not "bound" and is probably omitted?!
// TODO: My guess is that this function should look on all rules for a given function symbol
// TODO: (note that all left-hand sides use the same arguments!)
set<VariableIdx> getBoundVariables(const AbstractRule &rule, const VarMan &varMan) {
    set<VariableIdx> res;

    //updated variables are always bound
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        for (const auto &it : rhs->getUpdate()) {
            res.insert(it.first);
        }
    }

    //collect non-free variables from guard and cost
    ExprSymbolSet symbols;
    for (const Expression &ex : rule.getGuard()) {
        ex.collectVariables(symbols);
    }
    rule.getCost().collectVariables(symbols);

    for (const ExprSymbol &var : symbols) {
        if (!varMan.isTempVar(var)) {
            res.insert(varMan.getVarIdx(var));
        }
    }

    return res;
}

/**
 * Helper that prints the location's name or (if it has no name) its index to the given stream
 */
template <typename Rule>
void printLocation(LocationIdx loc, const AbstractITSProblem<Rule> &its, std::ostream &s) {
    s << COLOR_LOCATION;
    auto optName = its.getLocationName(loc);
    if (optName) {
        s << optName.get();
    } else {
        s << "[" << loc << "]";
    }
    s << COLOR_NONE;
}

/**
 * Helper that prints an entire rule in a human-readable format
 */
template <typename Rule>
void printRule(const AbstractRule &rule, const AbstractITSProblem<Rule> &its, std::ostream &s) {
    printLocation(rule.getLhsLoc(), its, s);
    s << " -> ";

    for (auto it = rule.rhsBegin(); it != rule.rhsEnd(); ++it) {
        printLocation(it->getLoc(), its, s);
        s << " : ";

        for (auto upit : it->getUpdate()) {
            s << COLOR_UPDATE;
            s << its.getVarName(upit.first) << "'";
            s << "=" << upit.second;
            s << COLOR_NONE;
            s << ", ";
        }
    }

    if (rule.getGuard().empty()) {
        s << "[]";
    } else {
        s << "[ ";
        for (int i=0; i < rule.getGuard().size(); ++i) {
            if (i > 0) s << " && ";
            s << COLOR_GUARD << rule.getGuard().at(i) << COLOR_NONE;
        }
        s << " ]";
    }
    s << ", cost: ";
    s << COLOR_COST << rule.getCost() << COLOR_NONE;
    s << endl;
}


template <typename Rule>
void ITSExport<Rule>::printLabeledRule(TransIdx rule, const AbstractITSProblem<Rule> &its, std::ostream &s) {
    s << setw(4) << rule << ": ";
    printRule(its.getRule(rule), its, s);
}


template <typename Rule>
void ITSExport<Rule>::printDebug(const AbstractITSProblem<Rule> &its, std::ostream &s) {
    s << "Variables:";
    for (VariableIdx i=0; i < its.getVariableCount(); ++i) {
        s << " " << its.getVarName(i);
        if (its.isTempVar(i)) {
            s << "*";
        }
    }
    s << endl;

    s << "Nodes:";
    for (LocationIdx loc : its.getLocations()) {
        s << " " << loc;
        auto optName = its.getLocationName(loc);
        if (optName) {
            s << "/" << optName.get();
        }
        if (its.isInitialLocation(loc)) {
            s << "*";
        }
    }
    s << endl;

    s << "Transitions:" << endl;
    for (LocationIdx loc : its.getLocations()) {
        for (TransIdx trans : its.getTransitionsFrom(loc)) {
            printLabeledRule(trans, its, s);
        }
    }
}


template <typename Rule>
void ITSExport<Rule>::printForProof(const AbstractITSProblem<Rule> &its, std::ostream &s) {
    s << "Start location: ";
    printLocation(its.getInitialLocation(), its, s);
    s << endl;

    if (its.isEmpty()) {
        s << "  <empty>" << endl;
        return;
    }

    for (LocationIdx n : its.getLocations()) {
        for (TransIdx trans : its.getTransitionsFrom(n)) {
            printLabeledRule(trans, its, s);
        }
    }
}


template <typename Rule>
void ITSExport<Rule>::printKoAT(const AbstractITSProblem<Rule> &its, std::ostream &s) {
    using namespace GiNaC;
    auto printNode = [&](LocationIdx n) {
        auto optName = its.getLocationName(n);
        if (optName) {
            s << optName.get();
        } else {
            s << "loc" << n << "'";
        }
    };

    s << "(GOAL COMPLEXITY)" << endl;
    s << "(STARTTERM (FUNCTIONSYMBOLS "; printNode(its.getInitialLocation()); s << "))" << endl;
    s << "(VAR";
    for (const ex &var : its.getGinacVarList()) {
        s << " " << var;
    }
    s << ")" << endl << "(RULES" << endl;

    for (LocationIdx n : its.getLocations()) {
        //write transition in KoAT format (note that relevantVars is an ordered set)
        for (TransIdx trans : its.getTransitionsFrom(n)) {
            const AbstractRule &rule = its.getRule(trans);
            set<VariableIdx> relevantVars = getBoundVariables(rule, its);

            //lhs
            printNode(n);
            bool first = true;
            for (VariableIdx var : relevantVars) {
                s << ((first) ? "(" : ",");
                s << its.getVarName(var);
                first = false;
            }

            //cost
            s << ") -{" << rule.getCost().expand() << "," << rule.getCost().expand() << "}> ";

            //rhs update
            if (rule.rhsCount() > 1) {
                s << "Com_" << rule.rhsCount() << "(";
            }

            for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
                if (rhs != rule.rhsBegin()) s << ",";
                printNode(rhs->getLoc());

                first = true;
                for (VariableIdx var : relevantVars) {
                    s << ((first) ? "(" : ",");
                    auto it = rhs->getUpdate().find(var);
                    if (it != rhs->getUpdate().end()) {
                        s << it->second.expand();
                    } else {
                        s << its.getVarName(var);
                    }
                    first = false;
                }
            }

            if (rule.rhsCount() > 1) {
                s << ")";
            }

            //guard
            s << ") :|: ";
            for (int i=0; i < rule.getGuard().size(); ++i) {
                if (i > 0) s << " && ";
                s << rule.getGuard().at(i).expand();
            }
            s << endl;
        }
    }
    s << ")" << endl;
}



void LinearITSExport::printDotSubgraph(const LinearITSProblem &its, int step, const std::string &desc, std::ostream &s) {
    auto printNode = [&](LocationIdx n) { s << "node_" << step << "_" << n; };

    s << "subgraph cluster_" << step << " {" << endl;
    s << "sortv=" << step << ";" << endl;
    s << "label=\"" << step << ": " << desc << "\";" << endl;
    for (LocationIdx n : its.getLocations()) {
        printNode(n); s << " [label=\""; printLocation(n, its, s); s << "\"];" << endl;
    }
    for (LocationIdx n : its.getLocations()) {
        for (LocationIdx succ : its.getSuccessorLocations(n)) {
            printNode(n); s << " -> "; printNode(succ); s << " [label=\"";
            for (TransIdx trans : its.getTransitionsFromTo(n,succ)) {
                const LinearRule &rule = its.getRule(trans);
                for (auto upit : rule.getUpdate()) {
                    s << its.getVarName(upit.first) << "=" << upit.second << ", ";
                }
                s << "[";
                for (int i=0; i < rule.getGuard().size(); ++i) {
                    if (i > 0) s << ", ";
                    s << rule.getGuard().at(i);
                }
                s << "], ";
                s << rule.getCost().expand(); //simplify for readability
                s << "\\l";
            }
            s << "\"];" << endl;
        }
    }
    s << "}" << endl;
}

void LinearITSExport::printDotText(int step, const std::string &txt, std::ostream &s) {
    s << "subgraph cluster_" << step << " {" << endl;
    s << "sortv=" << step << ";" << endl;
    s << "label=\"" << step << ": " << "Result" << "\";" << endl;
    s << "node_" << step << "_result [label=\"" << txt << "\"];" << endl;
    s << "}" << endl;
}

void LinearITSExport::printT2(const LinearITSProblem &its, std::ostream &s) {
    s << "START: 0;" << endl << endl;
    for (LocationIdx start : its.getLocations()) {
        for (TransIdx idx : its.getTransitionsFrom(start)) {
            const LinearRule &rule = its.getRule(idx);
            s << "FROM: " << start << ";" << endl;
            ExprSymbolSet vars = rule.getCost().getVariables();
            for (const Expression &ex : rule.getGuard()) {
                ex.collectVariables(vars);
            }
            for (auto it : rule.getUpdate()) {
                it.second.collectVariables(vars);
            }

            //create copy of vars ("pre vars") to simulate parallel assignments
            GiNaC::exmap t2subs;
            for (const ExprSymbol &sym : vars) {
                t2subs[sym] = GiNaC::symbol("pre_v" + sym.get_name());
                if (its.isTempVar(its.getVarIdx(sym))) {
                    s << t2subs[sym] << " := nondet();" << endl;
                } else {
                    s << t2subs[sym] << " := v" << sym.get_name() << ";" << endl;
                }
            }

            if (!rule.getGuard().empty()) {
                s << "assume(";
                for (int i=0; i < rule.getGuard().size(); ++i) {
                    if (i > 0) s << " && ";
                    s << rule.getGuard().at(i).subs(t2subs);
                }
                s << ");" << endl;
            }

            for (auto it : rule.getUpdate()) {
                s << "v" << its.getGinacSymbol(it.first) << " := ";
                s << it.second.subs(t2subs) << ";" << endl;
            }

            s << "TO: " << rule.getRhsLoc() << ";" << endl << endl;
        }
    }
}

