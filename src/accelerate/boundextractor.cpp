#include "boundextractor.hpp"

BoundExtractor::BoundExtractor(const GuardList &guard, const ExprSymbol &N): guard(guard), N(N) {
    extractBounds();
}

const option<Expression> BoundExtractor::getEq() const {
    return eq;
}

const std::vector<Expression> BoundExtractor::getLower() const {
    return lower;
}

const std::vector<Expression> BoundExtractor::getUpper() const {
    return upper;
}

const std::vector<Expression> BoundExtractor::getLowerAndUpper() const {
    std::vector<Expression> res;
    res.insert(res.end(), lower.begin(), lower.end());
    res.insert(res.end(), upper.begin(), upper.end());
    return res;
}

void BoundExtractor::extractBounds() {
    // First check if there is an equality constraint (we can then ignore all other upper bounds)
    for (const Expression &ex : guard) {
        if (Relation::isEquality(ex) && ex.has(N)) {
            auto optSolved = GuardToolbox::solveTermFor(ex.lhs() - ex.rhs(), N, GuardToolbox::ResultMapsToInt);
            if (optSolved) {
                // One equality is enough, as all other bounds must also satisfy this equality
                eq = optSolved.get();
            }
            return;
        }
    }

    // Otherwise, collect all bounds
    for (const Expression &ex : guard) {
        if (Relation::isEquality(ex) || !ex.has(N)) continue;

        Expression term = Relation::toLessEq(ex);
        term = (term.lhs() - term.rhs()).expand();
        if (term.degree(N) != 1) continue;

        // compute the upper bound represented by N and check that it is integral
        auto optSolved = GuardToolbox::solveTermFor(term, N, GuardToolbox::ResultMapsToInt);
        if (optSolved) {
            if (term.coeff(N, 1).info(GiNaC::info_flags::negative)) {
                lower.push_back(optSolved.get());
            } else {
                upper.push_back(optSolved.get());
            }
        }
    }
}

const ExpressionSet BoundExtractor::getConstantBounds() const {
    if (eq && eq.get().isIntegerConstant()) {
        return {eq.get()};
    }
    ExpressionSet res;
    for (const Expression &e: getLowerAndUpper()) {
        if (e.isIntegerConstant()) {
            res.insert(e);
        }
    }
    return res;
}
