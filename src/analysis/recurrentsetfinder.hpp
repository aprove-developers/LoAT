#ifndef RECURRENTSETFINDER_HPP
#define RECURRENTSETFINDER_HPP

#include "../its/itsproblem.hpp"
#include "../util/proof.hpp"

class RecurrentSetFinder
{
public:
    static void run(ITSProblem &its, bool evInc);
};

#endif // RECURRENTSETFINDER_HPP
