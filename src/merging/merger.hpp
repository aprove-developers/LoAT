#ifndef MERGER_HPP
#define MERGER_HPP

#include "../its/itsproblem.hpp"
#include "../util/proof.hpp"

class Merger
{
public:
    static Proof mergeRules(ITSProblem &its);

private:
    Merger(ITSProblem &its);

    ITSProblem &its;
    Proof proof;

    void merge();
    void merge(LocationIdx from, LocationIdx to);

};

#endif // MERGER_HPP
