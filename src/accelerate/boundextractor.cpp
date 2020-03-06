#include "boundextractor.hpp"

BoundExtractor::BoundExtractor(const GuardList &guard, const Var &N): guard(guard), N(N) {
    extractBounds();
}

const option<Expr> BoundExtractor::getEq() const {
    return eq;
}

const std::vector<Expr> BoundExtractor::getLower() const {
    return lower;
}

const std::vector<Expr> BoundExtractor::getUpper() const {
    return upper;
}

const std::vector<Expr> BoundExtractor::getLowerAndUpper() const {
    std::vector<Expr> res;
    res.insert(res.end(), lower.begin(), lower.end());
    res.insert(res.end(), upper.begin(), upper.end());
    return res;
}

void BoundExtractor::extractBounds() {
    // First check if there is an equality constraint (we can then ignore all other upper bounds)
    for (const Rel &rel : guard) {
        if (rel.relOp() == Rel::eq && rel.has(N)) {
            auto optSolved = GuardToolbox::solveTermFor(rel.lhs() - rel.rhs(), N, GuardToolbox::ResultMapsToInt);
            if (optSolved) {
                // One equality is enough, as all other bounds must also satisfy this equality
                eq = optSolved.get();
            }
            return;
        }
    }

    // Otherwise, collect all bounds
    for (const Rel &rel : guard) {
        if (rel.relOp() == Rel::eq || !rel.has(N)) continue;

        Rel leq = rel.toLeq();
        Expr term = (leq.lhs() - leq.rhs()).expand();
        if (term.degree(N) != 1) continue;

        // compute the upper bound represented by N and check that it is integral
        auto optSolved = GuardToolbox::solveTermFor(term, N, GuardToolbox::ResultMapsToInt);
        if (optSolved) {
            const Expr &coeff = term.coeff(N, 1);
            if (coeff.isRationalConstant() && coeff.toNum().is_negative()) {
                lower.push_back(optSolved.get());
            } else {
                upper.push_back(optSolved.get());
            }
        }
    }
}

const ExprSet BoundExtractor::getConstantBounds() const {
    if (eq && eq.get().isInt()) {
        return {eq.get()};
    }
    ExprSet res;
    for (const Expr &e: getLowerAndUpper()) {
        if (e.isInt()) {
            res.insert(e);
        }
    }
    return res;
}
