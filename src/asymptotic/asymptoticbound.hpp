#ifndef ASYMPTOTICBOUND_H
#define ASYMPTOTICBOUND_H

#include <string>
#include <vector>

#include "../expr/expression.hpp"
#include "../expr/guardtoolbox.hpp"
#include "../its/variablemanager.hpp"
#include "inftyexpression.hpp"
#include "limitproblem.hpp"
#include "../util/proof.hpp"


class AsymptoticBound {
private:
    // internal struct for the return value of getComplexity()
    struct ComplexityResult {
        ComplexityResult()
            : complexity(), upperBound(0), lowerBound(0), inftyVars(0) {
        }

        Subs solution;
        Complexity complexity;
        int upperBound;
        int lowerBound;
        int inftyVars;
    };

    AsymptoticBound(VarMan &varMan, Guard guard, Expr cost, bool finalCheck, unsigned int timeout);

    void initLimitVectors();
    void normalizeGuard();
    void createInitialLimitProblem(VariableManager &varMan);
    void propagateBounds();
    Subs calcSolution(const LimitProblem &limitProblem);
    int findUpperBoundforSolution(const LimitProblem &limitProblem, const Subs &solution);
    int findLowerBoundforSolvedCost(const LimitProblem &limitProblem, const Subs &solution);
    void removeUnsatProblems();
    bool solveViaSMT(Complexity currentRes);
    bool solveLimitProblem();
    ComplexityResult getComplexity(const LimitProblem &limitProblem);
    bool isAdequateSolution(const LimitProblem &limitProblem);

    void createBacktrackingPoint(const InftyExpressionSet::const_iterator &it, Direction dir);
    bool tryRemovingConstant(const InftyExpressionSet::const_iterator &it);
    bool tryTrimmingPolynomial(const InftyExpressionSet::const_iterator &it);
    bool tryReducingExp(const InftyExpressionSet::const_iterator &it);
    bool tryReducingGeneralExp(const InftyExpressionSet::const_iterator &it);
    bool tryApplyingLimitVector(const InftyExpressionSet::const_iterator &it);
    bool tryApplyingLimitVectorSmartly(const InftyExpressionSet::const_iterator &it);
    bool applyLimitVectorsThatMakeSense(const InftyExpressionSet::const_iterator &it,
                                        const Expr &l, const Expr &r,
                                        const std::vector<LimitVector> &limitVectors);
    bool tryInstantiatingVariable();
    bool trySubstitutingVariable();
    bool trySmtEncoding(Complexity currentRes);

private:
    VariableManager &varMan;
    const Guard guard;
    const Expr cost;
    bool finalCheck;
    Guard normalizedGuard;
    ComplexityResult bestComplexity;
    Proof proof;
    unsigned int timeout;

    std::vector<std::vector<LimitVector>> addition;
    std::vector<std::vector<LimitVector>> multiplication;
    std::vector<std::vector<LimitVector>> division;

    std::vector<LimitProblem> limitProblems;
    std::vector<LimitProblem> solvedLimitProblems;
    LimitProblem currentLP;

    std::vector<Subs> substitutions;

    std::vector<LimitVector> toApply;

public:
    /**
     * Result of the asymptotic complexity computation
     */
    struct Result {
        // The resulting complexity of the given rule.
        Complexity cpx;

        // The resulting cost, after expressing variables in terms of n.
        Expr solvedCost;

        // The number of non-constant variables (i.e., which grow with n).
        int inftyVars;

        Proof proof;

        explicit Result(Complexity c) : cpx(c), solvedCost(0), inftyVars(0) {}
        Result(Complexity c, Expr x, int v, Proof proof) : cpx(c), solvedCost(x), inftyVars(v), proof(proof) {}
    };

    /**
     * Analyzes the given guard and cost.
     * @param varMan the VariableManager instance is needed to get information about free variables
     * @param finalCheck enables more sophisticated backtracking and uses Timeout::hard
     */
    static Result determineComplexity(VarMan &varMan,
                                      const Guard &guard,
                                      const Expr &cost,
                                      bool finalCheck = false,
                                      const Complexity &currentRes = Complexity::Const,
                                      unsigned int timeout = Config::Smt::LimitTimeout);

    static Result determineComplexityViaSMT(VarMan &varMan,
                                            const Guard &guard,
                                            const Expr &cost,
                                            bool finalCheck = false,
                                            Complexity currentRes = Complexity::Const,
                                            unsigned int timeout = Config::Smt::LimitTimeout);

    static Result determineComplexityViaSMT(VarMan &varMan,
                                            const BoolExpr guard,
                                            const Expr &cost,
                                            bool finalCheck = false,
                                            Complexity currentRes = Complexity::Const,
                                            unsigned int timeout = Config::Smt::LimitTimeout);

};

#endif //ASYMPTOTICBOUND_H
