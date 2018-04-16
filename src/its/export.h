#ifndef EXPORT_H
#define EXPORT_H

#include <ostream>
#include <string>

#include "itsproblem.h"


template <typename Rule>
class ITSExport {
public:
    /**
     * Prints the ITS problem in a readable but ugly format for debugging
     */
    static void printDebug(const AbstractITSProblem<Rule> &its, std::ostream &s);

    /**
     * Print the ITS problem in a more readable format suitable for the proof output
     */
    static void printForProof(const AbstractITSProblem<Rule> &its, std::ostream &s);

    /**
     * Print the ITS problem in KoAT format (i.e. LoAT's input format)
     */
    static void printKoAT(const AbstractITSProblem<Rule> &its, std::ostream &s);
};


class LinearITSExport : public ITSExport<LinearRule> {
public:
    /**
     * Print the graph as partial dot output (only one subgraph is written)
     * @param s the output stream to print to (the dotfile)
     * @param step the number of the subgraph (should increase for every call)
     * @param desc a description of the current subgraph
     */
    static void printDotSubgraph(const LinearITSProblem &its, int step, const std::string &desc, std::ostream &s);

    /**
     * Similar to printDotSubgraph, but only prints text instead of a graph
     */
    static void printDotText(int step, const std::string &txt, std::ostream &s);

    /**
     * Print the graph in the T2 format, to allow conversion from koat -> T2.
     * @param s the output stream to be used
     */
    static void printT2(const LinearITSProblem &its, std::ostream &s);
};


class NonlinearITSExport : public ITSExport<NonlinearRule> {
};


// Since the implementation of AbstractITSProblem is not in the header,
// we have to explicitly declare all required instantiations.
template class ITSExport<LinearRule>;
template class ITSExport<NonlinearRule>;


#endif //EXPORT_H
