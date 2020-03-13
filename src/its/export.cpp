#include "export.hpp"
#include "../config.hpp"

using namespace std;
namespace Color = Config::Color;


// collects all variables appearing in the given rule
static void collectAllVariables(const Rule &rule, VarSet &vars) {
    for (auto rhs = rule.rhsBegin(); rhs != rule.rhsEnd(); ++rhs) {
        for (const auto &it : rhs->getUpdate()) {
            vars.insert(it.first);
            it.second.collectVars(vars);
        }
    }
    rule.getGuard().collectVariables(vars);
    rule.getCost().collectVars(vars);
}


// collects all non-temporary variables of the given rule
static void collectBoundVariables(const Rule &rule, const VarMan &varMan, VarSet &vars) {
    VarSet allVars;
    collectAllVariables(rule, allVars);

    for (const Var &var : allVars) {
        if (!varMan.isTempVar(var)) {
            vars.insert(var);
        }
    }
}


/**
 * Helper to display colors only if colored export is enabled.
 */
static void printColor(ostream &os, const std::string &s) {
    if (Config::Output::ColorsInITS) {
        os << s;
    }
}


/**
 * Helper that prints the location's name or (if it has no name) its index to the given stream
 */
static void printLocation(LocationIdx loc, const ITSProblem &its, std::ostream &s, bool colors) {
    if (colors) printColor(s, Color::Location);
    s << its.getPrintableLocationName(loc);
    if (colors) printColor(s, Color::None);
}


void ITSExport::printGuard(const GuardList &guard, std::ostream &s, bool colors) {
    if (colors) printColor(s, Color::None);
    if (guard.empty()) {
        s << "[]";
    } else {
        s << "[ ";
        for (unsigned int i=0; i < guard.size(); ++i) {
            if (i > 0) s << " && ";
            if (colors) printColor(s, Color::Guard);
            s << guard.at(i);
            if (colors) printColor(s, Color::None);
        }
        s << " ]";
    }
}


void ITSExport::printCost(const Expr &cost, std::ostream &s, bool colors) {
    if (colors) printColor(s, Color::Cost);
    s << cost;
    if (colors) printColor(s, Color::None);

}


void ITSExport::printRule(const Rule &rule, const ITSProblem &its, std::ostream &s, bool colors) {
    printLocation(rule.getLhsLoc(), its, s, colors);
    s << " -> ";

    for (auto it = rule.rhsBegin(); it != rule.rhsEnd(); ++it) {
        printLocation(it->getLoc(), its, s, colors);
        s << " : ";

        for (auto upit : it->getUpdate()) {
            if (colors) printColor(s, Color::Update);
            s << upit.first << "'";
            s << "=" << upit.second;
            if (colors) printColor(s, Color::None);
            s << ", ";
        }
    }

    printGuard(rule.getGuard(), s, colors);
    s << ", cost: ";
    printCost(rule.getCost(), s, colors);
}


void ITSExport::printLabeledRule(TransIdx rule, const ITSProblem &its, std::ostream &s) {
    s << setw(4) << rule << ": ";
    printRule(its.getRule(rule), its, s, true);
}


void ITSExport::printDebug(const ITSProblem &its, std::ostream &s) {
    s << "Variables:";
    for (const Var &x: its.getVars()) {
        s << " " << x;
        if (its.isTempVar(x)) {
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
            s << endl;
        }
    }
}


void ITSExport::printForProof(const ITSProblem &its, std::ostream &s) {
    s << "Start location: ";
    printLocation(its.getInitialLocation(), its, s, true);
    s << endl;

    if (its.isEmpty()) {
        s << "  <empty>" << endl;
        return;
    }

    for (LocationIdx n : its.getLocations()) {
        for (TransIdx trans : its.getTransitionsFrom(n)) {
            printLabeledRule(trans, its, s);
            s << endl;
        }
    }
}


void ITSExport::printKoAT(const ITSProblem &its, std::ostream &s) {
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
    VarSet vars;
    for (TransIdx rule : its.getAllTransitions()) {
        collectAllVariables(its.getRule(rule), vars);
    }
    for (const Var &var : vars) {
        s << " " << var;
    }

    s << ")" << endl << "(RULES" << endl;

    for (LocationIdx n : its.getLocations()) {
        // figure out which variables appear on the lhs of the given location
        VarSet relevantVars;
        for (TransIdx trans : its.getTransitionsFrom(n)) {
            collectBoundVariables(its.getRule(trans), its, relevantVars);
        }

        //write transition in KoAT format (note that relevantVars is an ordered set)
        for (TransIdx trans : its.getTransitionsFrom(n)) {
            const Rule &rule = its.getRule(trans);

            //lhs
            printNode(n);
            bool first = true;
            for (const Var &var : relevantVars) {
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
                for (const Var &var : relevantVars) {
                    s << ((first) ? "(" : ",");
                    auto it = rhs->getUpdate().find(var);
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
            for (unsigned int i=0; i < rule.getGuard().size(); ++i) {
                if (i > 0) s << " && ";
                s << rule.getGuard().at(i).expand();
            }
            s << endl;
        }
    }
    s << ")" << endl;
}



void LinearITSExport::printT2(const ITSProblem &its, std::ostream &s) {
    assert(its.isLinear());
    s << "START: 0;" << endl << endl;
    for (LocationIdx start : its.getLocations()) {
        for (TransIdx idx : its.getTransitionsFrom(start)) {
            const LinearRule rule = its.getLinearRule(idx);
            s << "FROM: " << start << ";" << endl;
            VarSet vars = rule.getCost().vars();
            for (const Rel &rel : rule.getGuard()) {
                rel.collectVariables(vars);
            }
            for (auto it : rule.getUpdate()) {
                it.second.collectVars(vars);
            }

            //create copy of vars ("pre vars") to simulate parallel assignments
            Subs t2subs;
            for (const Var &sym : vars) {
                t2subs.put(sym, Var("pre_v" + sym.get_name()));
                if (its.isTempVar(sym)) {
                    s << t2subs.get(sym) << " := nondet();" << endl;
                } else {
                    s << t2subs.get(sym) << " := v" << sym << ";" << endl;
                }
            }

            if (!rule.getGuard().empty()) {
                s << "assume(";
                for (unsigned int i=0; i < rule.getGuard().size(); ++i) {
                    if (i > 0) s << " && ";
                    s << rule.getGuard().at(i).subs(t2subs);
                }
                s << ");" << endl;
            }

            for (auto it : rule.getUpdate()) {
                s << "v" << it.first << " := ";
                s << it.second.subs(t2subs) << ";" << endl;
            }

            s << "TO: " << rule.getRhsLoc() << ";" << endl << endl;
        }
    }
}
