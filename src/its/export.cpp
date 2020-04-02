#include "export.hpp"
#include "../config.hpp"
#include "../expr/rel.hpp"

using namespace std;
namespace Color = Config::Color;


/**
 * Helper to display colors only if colored export is enabled.
 */
static void printColor(ostream &os, const std::string &s) {
    if (Config::Output::Colors) {
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


void ITSExport::printGuard(const BoolExpr &guard, std::ostream &s, bool colors) {
    if (colors) printColor(s, Color::Guard);
    s << guard;
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
