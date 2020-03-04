#ifndef BOUNDEXTRACTOR_H
#define BOUNDEXTRACTOR_H

#include <vector>
#include "../its/itsproblem.hpp"
#include "../expr/relation.hpp"
#include "../expr/guardtoolbox.hpp"

/**
 * Extracts bounds on the given variable from the given guard.
 */
class BoundExtractor
{
public:
    BoundExtractor(const GuardList &guard, const ExprSymbol &N);

    const option<Expression> getEq() const;
    const std::vector<Expression> getLower() const;
    const std::vector<Expression> getUpper() const;
    const std::vector<Expression> getLowerAndUpper() const;
    const ExpressionSet getConstantBounds() const;

private:

    void extractBounds();

    GuardList guard;
    ExprSymbol N;
    option<Expression> eq;
    std::vector<Expression> lower;
    std::vector<Expression> upper;

};

#endif // BOUNDEXTRACTOR_H
