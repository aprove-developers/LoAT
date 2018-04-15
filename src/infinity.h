/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#ifndef INFINITY_H
#define INFINITY_H

#include "global.h"
#include "expr/guardtoolbox.h"

#include <z3++.h>
#include <vector>
#include <map>

class ITRSProblem;
struct Transition;
class Expression;


/**
 * Class to handle the process of the final check for infinitely many models
 * @note currently, the implementation differs a bit from the paper, but should
 * always be a weaker than the approach presented there
 */
class InfiniteInstances {
public:
    //result type
    struct Result {
        Complexity cpx; //resulting total complexity
        bool reducedCpx; //runtime does not equal the given cost complexity (was reduced due to nonlinear subst.)
        Expression cost; //resulting cost term (where non-infty vars have been replaced by constants)
        int inftyVars;
        std::string reason;

        Result(Complexity c, std::string s) : cpx(c),cost(0),inftyVars(0),reason(s) {}
        Result(Complexity c, bool r, Expression x, int v, std::string s) : cpx(c),reducedCpx(r),cost(x),inftyVars(v),reason(s) {}
    };

    //internal types
    enum InftyDir { InftyBoth, InftyPos, InftyNeg, InftyConst };

    /**
     * This represents a possible configuration of all variables,
     * i.e. which variables can be positive/negative infinity, and how these variables are ordered (by absolute values)
     */
    class InftyCfg : public std::vector<InftyDir> {
        std::set<std::pair<int,int>> rel; //relations first > second (w.r.t. absolute values of Infty variables)
        bool isGreater(int A, int B) const;
    public:
        InftyCfg() {}
        InftyCfg(std::vector<InftyDir> && vec) : vector<InftyDir>(vec) {}
        bool addGreaterThan(int A, int B); //return true if added, false if not possible
        void removeConstRelations(); //remove relations where one variable is InftyConst
        const std::set<std::pair<int,int>>& getRelations() const { return rel; }
    };

    //internal typedefs
    class MonomData;
    typedef std::vector<MonomData> PolynomData;

    //internal result type
    struct CheckResult {
        Complexity cpx;
        InftyCfg cfg;
        bool reducedCpx;
        int inftyVars;
        GiNaC::exmap constSubs;
    };

public:
    //return complexity and number of infty variables in the used configuration
    /**
     * The main function to be called. Analyzes the given guard and cost expressions
     * @param itrs the ITRSProblem instance is needed to get information about free variables
     * @param isFinalCheck if true, the result is prepared for the proof output
     */
    static Result check(const ITRSProblem &itrs, GuardList guard, Expression cost, bool isFinalCheck);

private:
    InfiniteInstances(const ITRSProblem &itrs, GuardList guard, Expression cost);


    // ### Helpers ###

    //tiny helpers
    static bool setPos(InftyDir &dir);
    static bool setNeg(InftyDir &dir);
    static void setConst(InftyDir &dir);

    //more helpers
    static int getInftyVarCount(const InftyCfg &cfg);
    static int getExpSum(const MonomData &monom, const InftyCfg &cfg);
    int getUnboundedFreeExpSum(const MonomData &monom, const InftyCfg &cfg) const; //same as getExpSum, but only take free vars into account

    //even more little helpers
    int getVarCount() const { return symbols.size(); }
    InftyCfg getInitialConfig() const;

    //useful print/debug functions
    void printCfg(const InftyCfg &cfg, std::ostream &os = std::cout) const;
    void printMonom(const MonomData &monom) const;
    void printPolynom(const PolynomData &polynom) const;

    //debug dumping
    void dumpGuard(const std::string &description) const;
    void dumpConfigs(const std::set<InftyCfg> &configs) const;
    void dumpPolynoms() const;


    // ### Preprocessing Steps ###

    //symbol handling (maps internal variable indices to GiNaC symbols)
    void generateSymbolMapping();

    //handling for exponential results (this is done as preprocessing)
    bool getEXPterm(const Expression &term, Expression &res) const;
    bool replaceEXPrelation(const Expression relation, Expression &newrelation);
    bool replaceEXPguard();
    uint replaceEXPcost(); //returns level of nested exps (e.g. 0 if no exp is present)

    /**
     * Removes all equalities from the guard. Where possible, this is done using equality propagation.
     * This sets nonlinearSubs, which may have an important impact on the resulting complexity
     */
    void removeEqualitiesFromGuard();

    /**
     * Transforms polynomial relations into relations polynom >= 0
     */
    void makePolynomialGuard();

    /**
     * After makePolynomialGuard is run, this removes trivial inequations (e.g. 2 >= 0)
     * @return false iff there are trivial UNSAT inequations (i.e. -42 >= 0)
     */
    bool removeTrivialFromGuard(); //returns false if some trivial constraints are UNSAT (i.e. false)

    /**
     * Converts the given polynomial expression into the internal PolynomData representation
     */
    PolynomData parsePolynom(const Expression &term) const;
    void generatePolynomData();


    // ### Processing Steps ###

    /**
     * For a given polynomial and configuration, this returns a list of the monoms with the strongest influence on the complexity
     * (i.e. monoms with highest degrees).
     * @note free unbounded variables are preferred, as they may result in unbounded runtime
     * @return list of relevant monom indices (may be empty, if all monoms are constant)
     */
    std::vector<int> findRelevantMonoms(const PolynomData &polynom, const InftyCfg &cfg) const;

    /**
     * Ensures that the given monom is positive infinite (or constant) for the current cfg.
     * All resulting configurations (based on cfg) are added to res.
     *
     * This is the core method.
     * Example: y*z*x^2 with cfg [x: InftyNeg] will add [x: InftyNeg, y: InftyPos, z: InftyPos] and [x: InftyNeg, y: InftyNeg, z: InftyNeg] to res
     * (and probably also more configurations with x set to InftyConst as heuristic)
     */
    void addUpdatedConfigs(const MonomData &monom, const InftyCfg &cfg, std::set<InftyCfg> &res) const;

    /**
     * This simply calls addUpdatedConfigs for the given monom for each of the given configurations.
     * The resulting set of configurations is stored in configs again
     */
    void applyMonomToConfigs(const MonomData &monom, std::set<InftyCfg> &configs) const;

    /**
     * Similar to avove, applies all polynoms to the given configurations (both input and output parameter).
     * This involves the 3 methods listed above.
     */
    void applyPolynomsToConfigs(std::set<InftyCfg> &configs) const;

    /**
     * This implements a special heuristic to allow simple linear inequations, i.e. "A < B" with both variables positive infinity.
     * This requires imposing an ordering on the infinite variables. This method identifies cases where the heuristic is applicable.
     */
    void tryHeuristicForPosNegMonoms(const MonomData &monomA, const MonomData &monomB, const InftyCfg &cfg, std::set<InftyCfg> &configs) const;


    // ### Final Evaluation ###

    /**
     * Checks if the given configuration is valid (i.e. all constraints are infinite or positive constants).
     * To ensure that concrete constants exist, a Z3 query is performed (the result is stored in constSubs, if given).
     * @return true iff this configuration is valid
     */
    bool checkConfig(const InftyCfg &cfg, GiNaC::exmap *constSubs = nullptr) const;

    /**
     * This tries to set all variables not occuring in the cost polynomial to constants, where possible (to avoid reducing the final complexity)
     * @return true iff a valid configuration based on cfg was found (using checkConfig). Returns cfg and constSubs.
     */
    bool checkBestComplexity(InftyCfg &cfg, GiNaC::exmap *constSubs) const;

    /**
     * Returns true iff the given polynomial is always positive for the given configuration
     */
    bool isPositiveComplexity(const PolynomData &polynom, const InftyCfg &cfg) const;

    /**
     * Calculates the complexity for the given polynomial using the given configuration.
     * Note that nonlinearSubs are taken into account and may reduce the runtime (see example/sqrt)
     * @return the complexity and whether the runtime was reduced due to nonlinearsubs
     */
    std::pair<Complexity,bool> getEffectiveComplexity(const PolynomData &polynom, const InftyCfg &cfg) const;

    /**
     * Computes the final runtime complexity by finding the best of the given configurations, using the above methods.
     * @param costExpLvl The level of EXP-nesting that was removed during preprocessing (to distinguish EXP and EXP NESTED)
     * @param makeConstSubs if true, the constants found by Z3 are stored in the result (for usage in the proof output)
     */
    CheckResult calcTotalComplexity(const std::set<InftyCfg> &configs, uint costExpLvl, bool makeConstSubs) const;


    // ### Helper for final Complexity check ###

    //helper function for complexity (use isPositiveComplexity or getComplexity instead)
    /**
     * Returns the positive and negative complexity for the given polynomuial using the given variable configuration
     * (positive complexity is the max degree of all positive monoms, negative of monoms which may evaluate to negative values)
     */
    std::pair<Complexity,Complexity> calcComplexityPair(const PolynomData &polynom, const InftyCfg &cfg) const;

    /**
     * Returns the highest exponent of a non-constant variable in any nonlinearSubs (the complexity has to be reduced by this factor)
     */
    int getMaxNonlinearSubsDegree(const InftyCfg &cfg) const;

    /**
     * Returns true iff the monom using the given configuration contains unbounded free non-constant variables (i.e. complexity is unbouded)
     */
    bool containsUnboundedFreeInfty(const MonomData &monom, const InftyCfg &cfg) const;

    /**
     * A tiny helper to build the cost expression that is used in the proof output (currently just applies the given substitution)
     */
    Expression buildProofBound(const GiNaC::exmap &constSubs) const;

private:
    const ITRSProblem &itrs;

    //list of symbols / symbol to index mapping for all symbols that appear in the guard
    std::map<ExprSymbol,int,GiNaC::ex_is_less> symbolIndexMap;
    std::vector<ExprSymbol> symbols;

    //the guard and cost expressions (are modified)
    GuardList guard, originalGuard;
    Expression cost;
    Expression originalCost; //if EXP is replaced, this is the original term

    //nonlinear substitutions have to be taken into account for the final runtime
    //e.g. [ x == y^2 ] cost: x, with x/y^2 this seems to be quadratic, but really is only linear
    //e.g. [ y == x^2 ] cost: x, with y/x^2 this seems to be linear, but really is sqrt(n)... (as x is only a fraction of the input, which is y+x = x^2+x).
    GiNaC::exmap nonlinearSubs;

    //substitutions with free vars on rhs can lead to incorrect unbounded runtime, so avoid outputting INF in these cases
    //e.g. [ x == 2*free ], cost: x, with x/2free the cost is 2*free, i.e. unbounded, which is incorrect. So remember that free is not really free
    ExprSymbolSet freeBoundedVars;

    //if cost is exponential, this is the exponent
    PolynomData expPolynom;

    //the internal representation of the guard's polynomials
    std::vector<PolynomData> polynoms;
};

#endif // INFINITY_H
