#ifndef ASYMPTOTICBOUND_H
#define ASYMPTOTICBOUND_H

#include <string>
#include <vector>

#include "../expr/expression.hpp"
#include "../expr/guardtoolbox.hpp"
#include "../its/variablemanager.hpp"
#include "inftyexpression.hpp"
#include "limitproblem.hpp"
#include "../util/proofoutput.hpp"


class AsymptoticBound {
private:
    // internal struct for the return value of getComplexity()
    struct ComplexityResult {
        ComplexityResult()
            : complexity(), upperBound(0), lowerBound(0), inftyVars(0) {
        }

        GiNaC::exmap solution;
        Complexity complexity;
        int upperBound;
        int lowerBound;
        int inftyVars;
    };

    AsymptoticBound(VarMan &varMan, GuardList guard, Expression cost, bool finalCheck, uint timeout);

    void initLimitVectors();
    void normalizeGuard();
    void createInitialLimitProblem(VariableManager &varMan);
    void propagateBounds();
    GiNaC::exmap calcSolution(const LimitProblem &limitProblem);
    int findUpperBoundforSolution(const LimitProblem &limitProblem, const GiNaC::exmap &solution);
    int findLowerBoundforSolvedCost(const LimitProblem &limitProblem, const GiNaC::exmap &solution);
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
                                        const Expression &l, const Expression &r,
                                        const std::vector<LimitVector> &limitVectors);
    bool tryInstantiatingVariable();
    bool trySubstitutingVariable();
    bool trySmtEncoding(Complexity currentRes);

private:
    VariableManager &varMan;
    const GuardList guard;
    const Expression cost;
    bool finalCheck;
    GuardList normalizedGuard;
    ComplexityResult bestComplexity;
    ProofOutput proof;
    uint timeout;

    std::vector<std::vector<LimitVector>> addition;
    std::vector<std::vector<LimitVector>> multiplication;
    std::vector<std::vector<LimitVector>> division;

    std::vector<LimitProblem> limitProblems;
    std::vector<LimitProblem> solvedLimitProblems;
    LimitProblem currentLP;

    std::vector<GiNaC::exmap> substitutions;

    std::vector<LimitVector> toApply;

public:
    /**
     * Result of the asymptotic complexity computation
     */
    struct Result {
        // The resulting complexity of the given rule.
        Complexity cpx;

        // The resulting cost, after expressing variables in terms of n.
        Expression solvedCost;

        // The number of non-constant variables (i.e., which grow with n).
        int inftyVars;

        ProofOutput proof;

        explicit Result(Complexity c) : cpx(c), solvedCost(0), inftyVars(0) {}
        Result(Complexity c, Expression x, int v, ProofOutput proof) : cpx(c), solvedCost(x), inftyVars(v), proof(proof) {}
    };

    /**
     * Analyzes the given guard and cost.
     * @param varMan the VariableManager instance is needed to get information about free variables
     * @param finalCheck enables more sophisticated backtracking and uses Timeout::hard
     */
    static Result determineComplexity(VarMan &varMan,
                                      const GuardList &guard,
                                      const Expression &cost,
                                      bool finalCheck,
                                      const Complexity &currentRes,
                                      uint timeout);

    static Result determineComplexityViaSMT(VarMan &varMan,
                                            const GuardList &guard,
                                            const Expression &cost,
                                            bool finalCheck,
                                            Complexity currentRes,
                                            uint timeout);

};

#endif //ASYMPTOTICBOUND_H
