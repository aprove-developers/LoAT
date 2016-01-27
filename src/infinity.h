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
#include "guardtoolbox.h"

#include <z3++.h>
#include <vector>
#include <map>

class ITRSProblem;
struct Transition;
class Expression;


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

    //this represents a possible configuration of the variables
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
    static Result check(const ITRSProblem &itrs, GuardList guard, Expression cost, bool isFinalCheck);

private:
    InfiniteInstances(const ITRSProblem &itrs, GuardList guard, Expression cost);

    //helpers
    static bool setPos(InftyDir &dir);
    static bool setNeg(InftyDir &dir);
    static void setConst(InftyDir &dir);

    static int getInftyVarCount(const InftyCfg &cfg);
    static int getExpSum(const MonomData &monom, const InftyCfg &cfg);
    int getUnboundedFreeExpSum(const MonomData &monom, const InftyCfg &cfg) const; //same as getExpSum, but only take free vars into account

    int getVarCount() const { return symbols.size(); }
    InftyCfg getInitialConfig() const;

    //useful print functions
    void printCfg(const InftyCfg &cfg, std::ostream &os = std::cout) const;
    void printMonom(const MonomData &monom) const;
    void printPolynom(const PolynomData &polynom) const;

    //debug dumping
    void dumpGuard(const std::string &description) const;
    void dumpConfigs(const std::set<InftyCfg> &configs) const;
    void dumpPolynoms() const;

    //symbol handling
    void generateSymbolMapping();

    //handling for exponential results
    bool getEXPterm(const Expression &term, Expression &res) const;
    bool replaceEXPrelation(const Expression relation, Expression &newrelation);
    bool replaceEXPguard();
    uint replaceEXPcost(); //returns level of nested exps (e.g. 0 if no exp is present)

    //guard handling
    void removeEqualitiesFromGuard();
    void makePolynomialGuard();
    bool removeTrivialFromGuard(); //returns false if some trivial constraints are UNSAT (i.e. false)

    //guard to PolynomData
    PolynomData parsePolynom(const Expression &term) const;
    void generatePolynomData();

    //processing steps
    std::vector<int> findRelevantMonoms(const PolynomData &polynom, const InftyCfg &cfg) const;
    void addUpdatedConfigs(const MonomData &monom, const InftyCfg &cfg, std::set<InftyCfg> &res) const;
    void applyMonomToConfigs(const MonomData &monom, std::set<InftyCfg> &configs) const;
    void applyPolynomsToConfigs(std::set<InftyCfg> &configs) const;

    //heuristic: allow A > B by manually ensuring that A > B holds
    void tryHeuristicForPosNegMonoms(const MonomData &monomA, const MonomData &monomB, const InftyCfg &cfg, std::set<InftyCfg> &configs) const;

    //final evaluation
    bool checkConfig(const InftyCfg &cfg, GiNaC::exmap *constSubs = nullptr) const;
    bool checkBestComplexity(InftyCfg &cfg, GiNaC::exmap *constSubs) const;
    bool isPositiveComplexity(const PolynomData &polynom, const InftyCfg &cfg) const;
    std::pair<Complexity,bool> getEffectiveComplexity(const PolynomData &polynom, const InftyCfg &cfg) const;
    CheckResult calcTotalComplexity(const std::set<InftyCfg> &configs, uint costExpLvl, bool makeConstSubs) const;

    //helper function for complexity (use isPositiveComplexity or getComplexity instead)
    std::pair<Complexity,Complexity> calcComplexityPair(const PolynomData &polynom, const InftyCfg &cfg) const;
    int getMaxNonlinearSubsDegree(const InftyCfg &cfg) const;
    bool containsUnboundedFreeInfty(const MonomData &monom, const InftyCfg &cfg) const;

    //finally some nice proof output
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
