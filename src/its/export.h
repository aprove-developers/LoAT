#ifndef EXPORT_H
#define EXPORT_H

#include <ostream>
#include <string>

#include "itsproblem.h"


namespace ITSExport {
    /**
     * Print the given rule in a readable format, prefixed with its index (used for proof output)
     */
    void printLabeledRule(TransIdx rule, const ITSProblem &its, std::ostream &s);

    /**
     * Print the given rule in a readable format (used by printLabeledRule).
     * Note that colors are only used if colors is true and Config::Output::ColorInIts is true
     */
    void printRule(const Rule &rule, const ITSProblem &its, std::ostream &s, bool colors = true);

    /**
     * Prints the given guard in a readable format (constraints separated by &&)
     * Note that colors are only used if colors is true and Config::Output::ColorInIts is true
     */
    void printGuard(const GuardList &guard, std::ostream &s, bool colors = true);

    /**
     * Prints the given cost expression (does not do much except for colors)
     * Note that colors are only used if colors is true and Config::Output::ColorInIts is true
     */
    void printCost(const Expression &cost, std::ostream &s, bool colors = true);

    /**
     * Prints the ITS problem in a readable but ugly format for debugging
     */
    void printDebug(const ITSProblem &its, std::ostream &s);

    /**
     * Print the ITS problem in a more readable format suitable for the proof output
     */
    void printForProof(const ITSProblem &its, std::ostream &s);

    /**
     * Print the ITS problem in KoAT format (i.e. LoAT's input format)
     */
    void printKoAT(const ITSProblem &its, std::ostream &s);
}


namespace LinearITSExport {
    /**
     * Print the graph of a linear ITSProblem as partial dot output (only one subgraph is written)
     * @param its the ITSProblem to print, must be linear!
     * @param step the number of the subgraph (should increase for every call)
     * @param desc a description of the current subgraph
     * @param s the output stream to print to (the dotfile)
     */
    void printDotSubgraph(const ITSProblem &its, int step, const std::string &desc, std::ostream &s);

    /**
     * Similar to printDotSubgraph, but only prints text instead of a graph
     */
    void printDotText(int step, const std::string &txt, std::ostream &s);

    /**
     * Print the graph of a linear ITSProblem in the T2 format, to allow conversion from koat -> T2.
     * @param its the ITSProblem to print, must be linear!
     * @param s the output stream to be used
     */
    void printT2(const ITSProblem &its, std::ostream &s);
}


#endif //EXPORT_H
