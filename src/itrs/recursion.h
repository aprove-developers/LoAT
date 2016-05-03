#ifndef RECURSION_H
#define RECURSION_H

#include <set>

#include "expression.h"
#include "term.h"

class FunctionSymbol;
class ITRSProblem;
class RightHandSide;

class Recursion {
public:
    static bool solve(const ITRSProblem &itrs,
                      const FunctionSymbol &funSymbol,
                      std::set<RightHandSide*> rightHandSides,
                      Expression &result,
                      Expression &cost,
                      TermVector &guard);
};

#endif // RECURSION_H