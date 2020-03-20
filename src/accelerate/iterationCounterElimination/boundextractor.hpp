#ifndef BOUNDEXTRACTOR_H
#define BOUNDEXTRACTOR_H

#include <vector>
#include "../../its/itsproblem.hpp"
#include "../../expr/guardtoolbox.hpp"

/**
 * Extracts bounds on the given variable from the given guard.
 */
class BoundExtractor
{
public:
    BoundExtractor(const Guard &guard, const Var &N);

    const option<Expr> getEq() const;
    const std::vector<Expr> getLower() const;
    const std::vector<Expr> getUpper() const;
    const std::vector<Expr> getLowerAndUpper() const;
    const ExprSet getConstantBounds() const;

private:

    void extractBounds();

    Guard guard;
    Var N;
    option<Expr> eq;
    std::vector<Expr> lower;
    std::vector<Expr> upper;

};

#endif // BOUNDEXTRACTOR_H
