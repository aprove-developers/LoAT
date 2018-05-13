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


// collects all variables appearing in the given rule
static void collectAllVariables(const AbstractRule &rule, const VarMan &varMan, ExprSymbolSet &vars) {
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        for (const auto &it : rhs->getUpdate()) {
            vars.insert(varMan.getGinacSymbol(it.first));
            it.second.collectVariables(vars);
        }
    }
    rule.getGuard().collectVariables(vars);
    rule.getCost().collectVariables(vars);
}

// collects all non-temporary variables of the given rule
static void collectBoundVariables(const AbstractRule &rule, const VarMan &varMan, ExprSymbolSet &vars) {
    ExprSymbolSet allVars;
    collectAllVariables(rule, varMan, allVars);

    for (const ExprSymbol &var : allVars) {
        if (!varMan.isTempVar(var)) {
            vars.insert(var);
        }
    }
}


/**
 * Helper that prints the location's name or (if it has no name) its index to the given stream
 */
template <typename Rule>
static void printLocation(LocationIdx loc, const AbstractITSProblem<Rule> &its, std::ostream &s) {
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
static void printRule(const AbstractRule &rule, const AbstractITSProblem<Rule> &its, std::ostream &s, bool colors) {
    printLocation(rule.getLhsLoc(), its, s);
    s << " -> ";

    for (auto it = rule.rhsBegin(); it != rule.rhsEnd(); ++it) {
        printLocation(it->getLoc(), its, s);
        s << " : ";

        for (auto upit : it->getUpdate()) {
            if (colors) s << COLOR_UPDATE;
            s << its.getVarName(upit.first) << "'";
            s << "=" << upit.second;
            if (colors) s << COLOR_NONE;
            s << ", ";
        }
    }

    if (rule.getGuard().empty()) {
        s << "[]";
    } else {
        s << "[ ";
        for (int i=0; i < rule.getGuard().size(); ++i) {
            if (i > 0) s << " && ";
            if (colors) s << COLOR_GUARD;
            s << rule.getGuard().at(i);
            if (colors) s << COLOR_NONE;
        }
        s << " ]";
    }
    s << ", cost: ";
    if (colors) s << COLOR_COST;
    s << rule.getCost();
    if (colors) s << COLOR_NONE;
    s << endl;
}


template <typename Rule>
void ITSExport<Rule>::printLabeledRule(TransIdx rule, const AbstractITSProblem<Rule> &its, std::ostream &s) {
    s << setw(4) << rule << ": ";
    printRule(its.getRule(rule), its, s, true);
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

    // collect variables that actually appear in the rules
    ExprSymbolSet vars;
    for (TransIdx rule : its.getAllTransitions()) {
        collectAllVariables(its.getRule(rule), its, vars);
    }
    for (const ExprSymbol &var : vars) {
        s << " " << var;
    }

    s << ")" << endl << "(RULES" << endl;

    for (LocationIdx n : its.getLocations()) {
        // figure out which variables appear on the lhs of the given location
        ExprSymbolSet relevantVars;
        for (TransIdx trans : its.getTransitionsFrom(n)) {
            collectBoundVariables(its.getRule(trans), its, relevantVars);
        }

        //write transition in KoAT format (note that relevantVars is an ordered set)
        for (TransIdx trans : its.getTransitionsFrom(n)) {
            const AbstractRule &rule = its.getRule(trans);

            //lhs
            printNode(n);
            bool first = true;
            for (const ExprSymbol &var : relevantVars) {
                s << ((first) ? "(" : ",");
                s << var;
                first = false;
            }

            //cost
            s << ") -{" << rule.getCost().expand() << "," << rule.getCost().expand() << "}> ";

            //rhs updates
            if (rule.rhsCount() > 1) {
                s << "Com_" << rule.rhsCount() << "(";
            }

            for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
                if (rhs != rule.rhsBegin()) s << ",";
                printNode(rhs->getLoc());

                first = true;
                for (const ExprSymbol &var : relevantVars) {
                    s << ((first) ? "(" : ",");
                    auto it = rhs->getUpdate().find(its.getVarIdx(var));
                    if (it != rhs->getUpdate().end()) {
                        s << it->second.expand();
                    } else {
                        s << var;
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

