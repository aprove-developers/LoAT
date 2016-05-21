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

#include "infinity.h"

#include "timing.h"
#include "z3toolbox.h"
#include "guardtoolbox.h"
#include "itrs/recursiongraph.h"
#include "debug.h"

#include <queue>

using namespace std;
using namespace GiNaC;


/* ### Class to represent a variable configuration ### */

bool InfiniteInstances::InftyCfg::isGreater(int A, int B) const {
    for (const auto &p : rel) {
        if (p.first != A) continue;
        if (p.second == B) return true;
        else if (isGreater(p.second,B)) return true;
    }
    return false;
}

bool InfiniteInstances::InftyCfg::addGreaterThan(int A, int B) {
    if (rel.count(make_pair(A,B)) > 0) return true;
    if (rel.count(make_pair(B,A)) > 0) return false;
    if (isGreater(B,A)) return false;
    rel.insert(make_pair(A,B));
    return true;
}

void InfiniteInstances::InftyCfg::removeConstRelations() {
    set<pair<int,int>> remove;
    for (const auto &p : rel) {
        if ((*this)[p.first] == InftyConst || (*this)[p.second] == InftyConst) remove.insert(p);
    }
    for (const auto &p : remove) {
        rel.erase(p);
    }
}



/* ### Class to represent a multivariate monom, e.g. x^2y^5 ### */

typedef std::function<int(ExprSymbol)> SymToIntFunc;

class InfiniteInstances::MonomData {
    bool negative;
    vector<int> varExp; //maps variables to their exponents

public:
    MonomData(const Expression &term, int varCount, const SymToIntFunc &func)
        : negative(false), varExp(varCount,0)
    {
        Expression ex = term.expand();
        if (is_a<mul>(ex)) {
            for (int i=0; i < ex.nops(); ++i) parseSubexpr(ex.op(i),func);
        } else {
            parseSubexpr(ex,func);
        }
    }

    int getVarExp(int var) const { return varExp[var]; }
    int getVarExp(int var, const InfiniteInstances::InftyCfg &cfg) const { return (cfg[var] == InfiniteInstances::InftyConst) ? 0 : varExp[var]; }

    bool isNegative() const { return negative; }
    bool isSingleton(int &var) const {
        int foundVar = -1;
        for (int v=0; v < varExp.size(); ++v) {
            if (varExp[v] == 0) continue;
            if (foundVar >= 0) return false;
            foundVar = v;
        }
        var = foundVar;
        return true;
    }

    //returns true when this term is positive for every possible configuration given cfg
    bool isAlwaysPositive(const InfiniteInstances::InftyCfg &cfg) const {
        assert(cfg.size() == varExp.size());
        bool res = !negative;
        for (int i=0; i < cfg.size(); ++i) {
            if (varExp[i] % 2 == 0) continue;
            if (cfg[i] == InfiniteInstances::InftyBoth) return false; //config is not definite (probably because this monom is not relevant)
            if (cfg[i] == InfiniteInstances::InftyNeg) res = !res;
        }
        return res;
    }

private:
    void parsePower(const GiNaC::ex &base, const GiNaC::ex &exp, const SymToIntFunc &func) {
        assert(is_a<symbol>(base));
        assert(is_a<numeric>(exp));
        int expVal = ex_to<numeric>(exp).to_int();
        int var = func(ex_to<symbol>(base));
        assert(varExp[var] == 0);
        varExp[var] = expVal;
    }

    void parseSubexpr(const Expression &ex, const SymToIntFunc &func) {
        if (is_a<power>(ex)) {
            parsePower(ex.op(0),ex.op(1),func);
        } else if (is_a<numeric>(ex)) {
            if (ex_to<numeric>(ex).is_negative()) negative = !negative;
        } else if (is_a<symbol>(ex)) {
            parsePower(ex,1,func);
        }
    }
};



/* ### InfiniteInstances ### */

InfiniteInstances::InfiniteInstances(const ITRSProblem &its, GuardList guard, Expression cost)
    : its(its), guard(guard), originalGuard(guard), cost(cost), originalCost(cost) {}


/* ### Tiny Helpers ### */

bool InfiniteInstances::setPos(InftyDir &dir) {
    switch (dir) {
    case InftyBoth: dir = InftyPos; return true;
    case InftyNeg: dir = InftyConst; return true;
    default: return false;
    }
}

bool InfiniteInstances::setNeg(InftyDir &dir) {
    switch (dir) {
    case InftyBoth: dir = InftyNeg; return true;
    case InftyPos: dir = InftyConst; return true;
    default: return false;
    }
}

void InfiniteInstances::setConst(InftyDir &dir) {
    dir = InftyConst;
}

int InfiniteInstances::getInftyVarCount(const InftyCfg &cfg) {
    int res = 0;
    for (const InftyDir &dir : cfg) {
        if (dir != InftyConst) res++;
    }
    return res;
}

int InfiniteInstances::getExpSum(const MonomData &monom, const InftyCfg &cfg) {
    int res = 0;
    for (int i=0; i < cfg.size(); ++i) {
        res += monom.getVarExp(i,cfg);
    }
    return res;
}

int InfiniteInstances::getUnboundedFreeExpSum(const MonomData &monom, const InftyCfg &cfg) const {
    int res = 0;
    for (int i=0; i < cfg.size(); ++i) {
        if (!its.isFreeVar(its.getVarindex(symbols[i].get_name()))) continue; //not free
        if (freeBoundedVars.count(symbols[i]) > 0) continue; //free, but bounded
        res += monom.getVarExp(i,cfg);
    }
    return res;
}

InfiniteInstances::InftyCfg InfiniteInstances::getInitialConfig() const {
    return vector<InftyDir>(getVarCount(),InftyBoth);
}


void InfiniteInstances::printCfg(const InftyCfg &cfg, ostream &os) const {
    for (int i=0; i < cfg.size(); ++i) {
        if (i > 0) os << ", ";
        os << symbols[i] << ": ";
        switch (cfg[i]) {
        case InftyBoth: os << "Both"; break;
        case InftyPos:  os << "Pos";  break;
        case InftyNeg:  os << "Neg";  break;
        case InftyConst:os << "Const";break;
        }
    }
    auto rels = cfg.getRelations();
    if (!rels.empty()) {
        os << ", where:";
        for (const auto &p : rels) {
            os << " " << symbols[p.first] << " > " << symbols[p.second];
        }
    }
}

void InfiniteInstances::printMonom(const MonomData &monom) const {
    if (monom.isNegative()) cout << "-";
    for (int var=0; var < getVarCount(); ++var) {
        int exp = monom.getVarExp(var);
        if (exp > 0) cout << symbols[var] << "^" << exp << " ";
    }
}

void InfiniteInstances::printPolynom(const PolynomData &polynom) const {
    for (int i=0; i < polynom.size(); ++i) {
        printMonom(polynom[i]);
        cout << " ";
    }
}

void InfiniteInstances::dumpGuard(const string &description) const {
#ifdef DEBUG_INFINITY
    cout << description << ": ";
    for (auto ex : guard) cout << ex << " ";
    cout << "| " << cost;
    cout << endl;
#endif
}


void InfiniteInstances::dumpConfigs(const std::set<InftyCfg> &configs) const {
#ifdef DEBUG_INFINITY
    cout << "-------------------------------------------" << endl;
    cout << configs.size() << endl;
    for (InftyCfg c : configs) { printCfg(c); cout << endl; }
    cout << "-------------------------------------------" << endl;
#endif
}

void InfiniteInstances::dumpPolynoms() const {
#ifdef DEBUG_INFINITY
    cout << "###########################" << endl;
    for (int i=0; i < polynoms.size(); ++i) { printPolynom(polynoms[i]); cout << endl; }
    cout << "###########################" << endl;
#endif
}




InfiniteInstances::PolynomData InfiniteInstances::parsePolynom(const Expression &term) const {
    auto symToInt = [&](const ExprSymbol &sym){ return symbolIndexMap.at(sym); };
    vector<MonomData> res;
    Expression ex = term.expand();
    if (is_a<add>(ex)) {
        for (int i=0; i < ex.nops(); ++i) res.push_back(MonomData(ex.op(i),getVarCount(),symToInt));
    } else {
        res.push_back(MonomData(ex,getVarCount(),symToInt));
    }
    assert(!res.empty());
    return res;
}


//NOTE: may return empty list!
vector<int> InfiniteInstances::findRelevantMonoms(const PolynomData &polynom, const InftyCfg &cfg) const {
    assert(!polynom.empty());
    vector<int> res;

    //first search for terms containing free variables, as they are unbounded by the input
    bool only_free = true;
    int maxExp = getUnboundedFreeExpSum(polynom[0],cfg);
    for (int i=1; i < polynom.size(); ++i) {
        maxExp = max(maxExp,getUnboundedFreeExpSum(polynom[i],cfg));
    }

    //if there are no such terms, interpret free variables as regular variables and search again
    if (maxExp == 0) {
        only_free = false;
        maxExp = getExpSum(polynom[0],cfg);
        for (int i=1; i < polynom.size(); ++i) {
            maxExp = max(maxExp,getExpSum(polynom[i],cfg));
        }
    }

    //still no relevant monom (polynom is const)
    if (maxExp == 0) return {};

    //return all monoms that have maximal exp sum
    for (int i=0; i < polynom.size(); ++i) {
        if ((only_free ? getUnboundedFreeExpSum(polynom[i],cfg) : getExpSum(polynom[i],cfg)) == maxExp)
            res.push_back(i);
    }
    return res;
}


void InfiniteInstances::addUpdatedConfigs(const MonomData &monom, const InftyCfg &cfg, set<InftyCfg> &res) const {
#ifdef DEBUG_INFINITY
    cout << "    =======================" << endl;
    cout << "    MONOM: "; printMonom(monom); cout << endl;
    cout << "    IN: "; printCfg(cfg); cout << endl;
#endif

    //Acts as a counting register to iterate over all possible variable assignments
    vector<char> data(getVarCount(),0); //0: not present; 1: positive; 2: negative; 4: equal exponent

    //find relevant variables, i.e. with odd exponent
    for (int var=0; var < getVarCount(); ++var) {
        int exp = monom.getVarExp(var,cfg);
        data[var] = (exp == 0) ? 0 : ((exp % 2 == 1) ? 1 : 4);
    }

    bool squares_const = false; //true iff variables with even exponent are set to const
    bool found = false;
    while (true) {
        //check current sign and calculate new config
        InftyCfg newcfg = cfg;
        bool neg = monom.isNegative();
        bool sign_infty = false; //true iff the sign is influenced by infty variables
        for (int var=0; var < getVarCount(); ++var) {
            if (data[var] == 2) neg = !neg;
            if (data[var] == 1) sign_infty = setPos(newcfg[var]) || sign_infty;
            if (data[var] == 2) sign_infty = setNeg(newcfg[var]) || sign_infty;
            if (squares_const && data[var] == 4) setConst(newcfg[var]);
        }
        newcfg.removeConstRelations();
        //add config if this assignment results in a positive term
        if (neg == false || (squares_const && !sign_infty)) {
            res.insert(newcfg);
            found = true;
            debugInfinity("    ADD: "; printCfg(newcfg); cout);
        }
        //advance to next assignment (or abort)
        for (int i=getVarCount()-1; i >= 0; --i) {
            if (data[i] == 2) data[i] = 1;
            else if (data[i] == 1) {
                data[i] = 2; //use other direction
                goto next;
            }
        }
        //abort if found, or continue and allow setting irrelevant variables to const
        if (found || squares_const) break;
        squares_const = true;
        for (int var=0; var < getVarCount(); ++var) {
            if (data[var] == 2) data[var] = 1;
        }
        //continue
        next:;
    }

    debugInfinity("    =======================");
}


bool InfiniteInstances::getEXPterm(const Expression &term, Expression &exp) const {
    using namespace GiNaC;

    //get exp term
    struct PowVisitor : public visitor, public power::visitor {
        void visit(const power &p) {
            assert(p.nops() == 2);
            if (found) return;
            if (is_a<numeric>(p.op(1))) return;
            found = true;
            expTerm = p;
        }
        bool hasFound() const { return found; }
        const Expression &getFirstExp() const { return expTerm; }
    private:
        bool found=false;
        Expression expTerm;
    };

    PowVisitor v;
    term.traverse(v);
    if (!v.hasFound()) return false;
    exp = v.getFirstExp();

    return true;
}


bool InfiniteInstances::replaceEXPrelation(const Expression relation, Expression &newrelation) {
    using namespace GiNaC;

    if (GuardToolbox::isEquality(relation)) return false; //exp == rhs not allowed
    Expression term = GuardToolbox::makeLessEqual(relation);
    term = term.rhs() - term.lhs(); //rhs-lhs >= 0

    Expression exp;
    if (!getEXPterm(term,exp)) return false;
    if (!exp.op(0).is_polynomial(its.getGinacVarList())) return false; //we allow 2^(2^x), but not (2^x)^(2^x)

    //add to guard to ensure a proper base (might be trivial, e.g. 2 >= 2)
    guard.push_back(exp.op(0) >= 2);

    //add to guard to ensure that the coefficient does not reduce the exp value
    ex coeff = term.coeff(exp);
    guard.push_back(coeff >= 0);

    //we use the fact that e^poly >= poly and thus only require poly >= term to have e^poly >= term.
    term = - (term - coeff*exp); //move all but exp to rhs, i.e. exp >= rhs. Remove coeff (checked individually)
    newrelation = exp.op(1) >= term;

    return true;
}

bool InfiniteInstances::replaceEXPguard() {
    bool res = false;
    int len = guard.size();
    for (int i=0; i < len; ++i) {
        bool changed = true;
        //call multiple times to resolve 2^(2^x), but limit number of steps to avoid nontermination (for very weird terms...)
        for (int step=0; changed && step < 10; ++step) {
            Expression newguard;
            changed = replaceEXPrelation(guard[i],newguard); //may append to guard
            if (changed) guard[i] = newguard;
            res |= changed;
        }
    }
    return res;
}

uint InfiniteInstances::replaceEXPcost() {
    uint res = 0;
    Expression exp;
    while (true) {
        if (!getEXPterm(cost,exp)) break;
        if (!exp.op(0).is_polynomial(its.getGinacVarList())) break; //we allow 2^(2^x), but not (2^x)^(2^x)
        res += 1; //found one more exp level

        //add to guard to ensure a proper base (might be trivial, e.g. 2 >= 2)
        guard.push_back(exp.op(0) >= 2);

        //add to guard to ensure that the coefficient does not reduce the exp value
        ex coeff = cost.coeff(exp);
        guard.push_back(coeff >= 0);

        //modify cost term to replace exp^poly by poly
        cost = cost - (coeff*exp) + exp.op(1);
    }
    if (res > 0) {
        //setup polynom data to convert to exp runtime later (in case of 2^(2^x) we only use x here)
        expPolynom = parsePolynom(exp.op(1));
    }
    return res;
}



void InfiniteInstances::removeEqualitiesFromGuard() {
    //propagate equalities where possible
    GiNaC::exmap equalSubs;
    GuardToolbox::propagateEqualities(its,guard,GuardToolbox::NoCoefficients,GuardToolbox::AllowFreeOnRhs,&equalSubs);
    cost = cost.subs(equalSubs); //substitution must also be applied to cost

    //find free variables that are on rhs of substitutions, so they are in fact bounded
    for (const auto &it : equalSubs) {
        if (its.isFreeVar(its.getVarindex(GiNaC::ex_to<GiNaC::symbol>(it.first).get_name()))) continue; //free/free2 imposes no bounds on anything
        for (string varname : Expression(it.second).getVariableNames()) {
            VariableIndex vi = its.getVarindex(varname);
            if (its.isFreeVar(vi)) freeBoundedVars.insert(its.getGinacSymbol(vi));
        }
    }

    //find nonlinear substitutions, as they have an impact on the resulting runtime complexity
    for (const auto &it : equalSubs) {
        assert(it.second.is_polynomial(its.getGinacVarList()));

        //substituting (truly) free variables is ok
        string varname = GiNaC::ex_to<GiNaC::symbol>(it.first).get_name();
        if (its.isFreeVar(its.getVarindex(varname)) && freeBoundedVars.count(GiNaC::ex_to<symbol>(it.first)) == 0) continue;

        //but otherwise, we need to remember all nonlinear substs
        if (!Expression(it.second).isLinear(its.getGinacVarList())) {
            nonlinearSubs[it.first] = it.second;
        }
    }

    //manually replace == by <= and >= for all remaining equalities
    for (int i=0; i < guard.size(); ++i) {
        assert(is_a<relational>(guard[i]));
        if (GuardToolbox::isEquality(guard[i])) {
            guard.push_back(guard[i].lhs() <= guard[i].rhs());
            guard.push_back(guard[i].lhs() >= guard[i].rhs());
            guard.erase(guard.begin() + i);
            i--;
        }
    }
}


void InfiniteInstances::makePolynomialGuard() {
    for (int i=0; i < guard.size(); ++i) {
        Expression tmp = GuardToolbox::makeLessEqual(guard[i]); //lhs <= rhs
        guard[i] = tmp.rhs()-tmp.lhs(); //rhs-lhs >= 0
    }
}


bool InfiniteInstances::removeTrivialFromGuard() {
    for (int i=0; i < guard.size(); ++i) {
        if (is_a<numeric>(guard[i])) {
            numeric num = ex_to<numeric>(guard[i]);
            if (num.is_negative()) {
                return false; //trivial constraint is false (i.e. UNSAT)
            }
            guard.erase(guard.begin()+i);
            i--;
        }
    }
    return true;
}


void InfiniteInstances::generateSymbolMapping() {
    //find all variables that occur in the polyguard/cost
    ExprSymbolSet symset = cost.getVariables();
    for (const Expression &ex : guard) ex.collectVariables(symset);
    //fill symbol list and symbol index mapping
    for (ExprSymbol sym : symset) {
        symbolIndexMap[sym] = symbols.size();
        symbols.push_back(sym);
    }
}


void InfiniteInstances::generatePolynomData() {
    //parse all guard terms and the cost to polynom data
    for (const Expression &ex : guard) {
        polynoms.push_back(parsePolynom(ex));
    }
    polynoms.push_back(parsePolynom(cost));
}


void InfiniteInstances::applyMonomToConfigs(const MonomData &monom, set<InftyCfg> &configs) const {
    set<InftyCfg> next;
    for (InftyCfg c : configs) {
        addUpdatedConfigs(monom,c,next);
    }
    configs.swap(next);
}


void InfiniteInstances::tryHeuristicForPosNegMonoms(const MonomData &monomA, const MonomData &monomB, const InftyCfg &cfg, set<InftyCfg> &configs) const {
    int A,B;
    if (monomA.isSingleton(A) && monomB.isSingleton(B) && monomA.isNegative() != monomB.isNegative()) {
        if (monomA.getVarExp(A,cfg) == monomB.getVarExp(B,cfg) && monomA.getVarExp(A,cfg) > 0) {
            //if exponent is even, the actual direction is irrelevant
            bool expOdd = (monomA.getVarExp(A,cfg) % 2 == 1);

            //if sign is swapped, the relation must also be swapped
            if (monomA.isNegative()) std::swap(A,B);

            InftyCfg newCfg = cfg;
            if (expOdd) {
                setPos(newCfg[A]);
                setPos(newCfg[B]);
            }
            if (newCfg[A] != InftyConst && newCfg[B] != InftyConst && newCfg.addGreaterThan(A,B)) {
                configs.insert(newCfg);
                debugInfinity("    ADD HEURISTIC: "; printCfg(newCfg); cout);
            }

            newCfg = cfg;
            if (expOdd) {
                setNeg(newCfg[A]);
                setNeg(newCfg[B]);
            }
            if (newCfg[A] != InftyConst && newCfg[B] != InftyConst && newCfg.addGreaterThan(B,A)) {
                configs.insert(newCfg);
                debugInfinity("    ADD HEURISTIC: "; printCfg(newCfg); cout);
            }
        }
    }
}


void InfiniteInstances::applyPolynomsToConfigs(set<InftyCfg> &configs) const {
    set<InftyCfg> currConfigs;
    do {
        currConfigs = configs; //copy
        set<InftyCfg> nextConfigs;
        for (int i=0; i < polynoms.size(); ++i) {
            debugInfinity("  ++++++++++++++++++++++++++++++++++++++++");
            for (const InftyCfg &cfg : currConfigs) {
                //find all monoms with the highest exponent sum
                vector<int> monomIdx = findRelevantMonoms(polynoms[i],cfg);
                //apply those monoms' restrictions to the current configuration
                set<InftyCfg> updatedCfg = {cfg};
                for (int idx : monomIdx) {
                    applyMonomToConfigs(polynoms[i][idx],updatedCfg);
                }
                nextConfigs.insert(updatedCfg.begin(),updatedCfg.end());

                //special case to allow constraints of the form A > B, i.e. for A-B > 0 allow A,B to be both InftyPos or InftyNeg
                if (monomIdx.size() == 2) {
                    const MonomData &monomA = polynoms[i][monomIdx[0]];
                    const MonomData &monomB = polynoms[i][monomIdx[1]];
                    tryHeuristicForPosNegMonoms(monomA,monomB,cfg,nextConfigs);
                }
            }
            currConfigs.clear();
            currConfigs.swap(nextConfigs);
        }
        configs.swap(currConfigs);

        dumpConfigs(configs);

    } while (currConfigs != configs);
}


bool InfiniteInstances::containsUnboundedFreeInfty(const MonomData &monom, const InftyCfg &cfg) const {
    for (int var=0; var < cfg.size(); ++var) {
        if (cfg[var] == InftyConst) continue;
        if (freeBoundedVars.count(symbols[var]) > 0) continue; //free, but bounded
        if (its.isFreeVar(its.getVarindex(symbols[var].get_name()))) {
            if (monom.getVarExp(var) > 0) return true;
        }
    }
    return false;
}

pair<Complexity,Complexity> InfiniteInstances::calcComplexityPair(const PolynomData &polynom, const InftyCfg &cfg) const {
    Complexity cpxPos=0, cpxNeg=-1;
    for (const MonomData &monom : polynom) {
        Complexity cpx = getExpSum(monom,cfg);
        if (containsUnboundedFreeInfty(monom,cfg)) {
            cpx = Expression::ComplexInfty;
        }
        if (monom.isAlwaysPositive(cfg)) {
            if (cpx > cpxPos) cpxPos = cpx;
        } else {
            if (cpx > cpxNeg) cpxNeg = cpx;
        }
    }
    return make_pair(cpxPos,cpxNeg);
}


bool InfiniteInstances::isPositiveComplexity(const PolynomData &polynom, const InftyCfg &cfg) const {
    Complexity cpxPos,cpxNeg;
    tie(cpxPos,cpxNeg) = calcComplexityPair(polynom,cfg);
    return (cpxPos > cpxNeg && cpxPos > 0);
}

int InfiniteInstances::getMaxNonlinearSubsDegree(const InftyCfg &cfg) const {
    //find all variables that are non const
    ExprList checkVars;
    for (int var=0; var < cfg.size(); ++var) {
        if (cfg[var] != InftyConst) checkVars.append(symbols[var]);
    }
    //find max degree on any subst rhs for these variables
    int res = 1;
    for (const auto &it : nonlinearSubs) {
        Expression rhs = it.second;
        int deg = rhs.getMaxDegree(checkVars);
        if (deg > res) res = deg;
    }
    return res;
}

pair<Complexity,bool> InfiniteInstances::getEffectiveComplexity(const PolynomData &polynom, const InftyCfg &cfg) const {
    Complexity cpxPos,cpxNeg;
    tie(cpxPos,cpxNeg) = calcComplexityPair(polynom,cfg);

    //this can happen if free variables occur unbounded and thus make the polynom negative unbounded
    if (cpxNeg == Expression::ComplexInfty) {
        return make_pair(Expression::ComplexNone,false);
    }

    //if cpxPos,cpxNeg are both zero, the guard is either trivial or constants occur,
    //which are checked in another place. Nonzero higher negative cpx should not occur!

    //this condition is now violated, as we allow A-B (where we ensure A > B by InftyCfg.rel)
//    assert(cpxPos > cpxNeg || cpxNeg <= 0);

    //this should still hold, however:
    assert(cpxPos >= cpxNeg || cpxNeg <= 0);

    //if we applied nonlinear substitutions, reduce the final complexit
    int maxSubstDeg = getMaxNonlinearSubsDegree(cfg);
    assert(maxSubstDeg >= 1);
    return make_pair(cpxPos.divInt(maxSubstDeg),maxSubstDeg != 1);
}



bool InfiniteInstances::checkConfig(const InftyCfg &cfg, GiNaC::exmap *constSubs) const {
#ifdef DEBUG_INFINITY
    debugInfinity("Checkking config: ");
    printCfg(cfg); cout << endl;
#endif

    //check if there are non-const variables
    for (int i=0; i < cfg.size(); ++i) {
        if (cfg[i] != InftyConst) goto nonconst;
    }
    return false;

nonconst:
    //check if the const variables are allowed to be positive!
    bool checkCost = false;
    ExprSymbolSet checkVars;
    GuardList checkGuard;
    for (int i=0; i <= guard.size(); ++i) { //i includes cost
        const Expression &ex = (i < guard.size()) ? guard[i] : cost;
        ExprSymbolSet exVars = ex.getVariables();
        for (const ExprSymbol &sym : exVars) {
            if (cfg[symbolIndexMap.at(sym)] == InftyConst) {
                checkVars.insert(exVars.begin(),exVars.end());
                checkGuard.push_back(ex >= 0);
                if (i == guard.size()) checkCost = true;
                break;
            }
        }
    }
    if (!checkGuard.empty()) {
        for (const ExprSymbol &sym : checkVars) {
            int idx = symbolIndexMap.at(sym);
            if (cfg[idx] == InftyBoth) continue;
            checkGuard.insert(checkGuard.begin(),(cfg[idx] == InftyNeg) ? (sym < 0) : (sym > 0));
        }

        debugInfinity("z3 check sat: "; for (auto ex : checkGuard) cout << ex << " "; cout);

        Z3VariableContext context;
        z3::model model(context,Z3_model());
        auto z3res = Z3Toolbox::checkExpressionsSAT(checkGuard,context,&model);

        //try to cheat on z3
        if (z3res == z3::unknown && checkCost) {
            //cost is often a complicated expression, so we handle that one on our own
            checkGuard.pop_back();
            debugInfinity("z3 check again: "; for (auto ex : checkGuard) cout << ex << " "; cout);

            z3res = Z3Toolbox::checkExpressionsSAT(checkGuard,context,&model);

            //we still have to check if cost is ok with these constants
            if (z3res == z3::sat) {
                GiNaC::exmap costSubs;
                for (const ExprSymbol &sym : checkVars) {
                    if (cfg[symbolIndexMap.at(sym)] == InftyConst) {
                        costSubs[sym] = Z3Toolbox::getRealFromModel(model,Expression::ginacToZ3(sym,context));
                    }
                }
                Expression newCost = cost.subs(costSubs);
                debugInfinity("Checking cost with z3 model consts: " << newCost);
                if (!isPositiveComplexity(parsePolynom(newCost),cfg)) {
                    z3res = z3::unknown; //check failed, we have no idea about the result
                }
            }

            //add back cost for next fallback checks (good luck...)
            checkGuard.push_back(cost >= 0);
        }

        //let's try some simpler cheats
        if (z3res == z3::unknown) {
            //first try expanding all terms to ease computation
            for (Expression &ex : checkGuard) ex = ex.expand();
            debugInfinity("z3 check again2: "; for (auto ex : checkGuard) cout << ex << " "; cout);
            z3res = Z3Toolbox::checkExpressionsSAT(checkGuard,context,&model);
        }
        if (z3res == z3::unknown) {
            //z3 failed again, let's try setting all consts to 1
            for (const ExprSymbol &sym : checkVars) {
                int idx = symbolIndexMap.at(sym);
                if (cfg[idx] == InftyConst) checkGuard.insert(checkGuard.begin(),sym == 1);
            }
            debugInfinity("z3 check again3: "; for (auto ex : checkGuard) cout << ex << " "; cout);
            z3res = Z3Toolbox::checkExpressionsSAT(checkGuard,context,&model);
        }

        if (z3res != z3::sat) {
            debugInfinity("Discarding cfg as it has const vars and z3 is: " << z3res);
            return false;
        }

        //output the values of all constant variables
        if (constSubs) {
            for (int i=0; i < cfg.size(); ++i) {
                if (cfg[i] != InftyConst) continue;
                if (checkVars.count(symbols[i]) == 0) continue;
                (*constSubs)[symbols[i]] = Z3Toolbox::getRealFromModel(model,Expression::ginacToZ3(symbols[i],context));
            }
        }
    }
    return true;
}


bool InfiniteInstances::checkBestComplexity(InftyCfg &cfg, GiNaC::exmap *constSubs) const {
    //if the config does not work at all, abort early
    if (!checkConfig(cfg,nullptr)) return false;

    //find variables we want to be const (to avoid reducing the final runtime)
    typedef pair<int,int> VarDeg;
    auto comp = [](VarDeg a, VarDeg b) { return a.second < b.second; }; //the higher the degree, the worse is the resulting runtime
    priority_queue<VarDeg,std::vector<VarDeg>,decltype(comp)> badvar(comp);

    for (const auto &it : nonlinearSubs) {
        Expression rhs(it.second);
        for (ExprSymbol sym : rhs.getVariables()) {
            if (cost.has(sym)) continue; //variables appearing in cost should still be Infty (greedy heuristic)
            int var = symbolIndexMap.at(sym);
            if (cfg[var] != InftyConst) {
                GiNaC::lst lst; lst.append(sym);
                int deg = rhs.getMaxDegree(lst);
                badvar.push(make_pair(var,deg));
            }
        }
    }

    //try setting as many bad variables to InftyConst as possible (currently only a weak heuristic: sort by most influential variables)
    while (!badvar.empty()) {
        int var = badvar.top().first;
        badvar.pop();
        InftyDir oldDir = cfg[var];

        cfg[var] = InftyConst;
        if (!checkConfig(cfg,nullptr)) {
            cfg[var] = oldDir; //const is not ok, restore
        }
    }

    //this should be true, as we restored all problematic cases
    bool res = checkConfig(cfg,constSubs);
    assert(res);
    return res;
}


InfiniteInstances::CheckResult InfiniteInstances::calcTotalComplexity(const set<InftyCfg> &configs, uint costExpLvl, bool makeConstSubs) const {
    CheckResult best;
    best.reducedCpx = false;
    best.cpx = Expression::ComplexNone;
    best.inftyVars = 0;
    for (InftyCfg cfg : configs) {
        GiNaC::exmap subs;
        if (!checkBestComplexity(cfg,makeConstSubs ? &subs : nullptr)) continue;

        auto effCpx = getEffectiveComplexity(*polynoms.rbegin(),cfg); //cost
        Complexity cpx = effCpx.first;
        int inftyVars = getInftyVarCount(cfg);

        if (costExpLvl > 0) {
            effCpx = getEffectiveComplexity(expPolynom,cfg);
            if (effCpx.first > 0) {
                cpx = (costExpLvl == 1) ? Expression::ComplexExp : Expression::ComplexExpMore;
            }
        }

        if (cpx > best.cpx || (cpx == best.cpx && inftyVars > best.inftyVars)) {
            best.cpx = cpx;
            best.reducedCpx = effCpx.second;
            best.inftyVars = inftyVars;
            best.cfg = cfg;
            if (makeConstSubs) best.constSubs = std::move(subs);
        }
    }
#ifdef DEBUG_INFINITY
    if (best.cpx >= 0) {
        cout << "Success: Complexity " << best.cpx << " with configuration:" << endl;
        printCfg(best.cfg); cout << endl;
    }
#endif
    return best;
}


Expression InfiniteInstances::buildProofBound(const exmap &constSubs) const {
    //no interesting substitutions happened, we can just output the original cost function, with InftyConst vars replaced
    if (nonlinearSubs.empty()) {
        return originalCost.subs(constSubs); //originalCost may contain exponentials, so use this one
    }

    //in the other cases, we currently still output the same cost (together with a note for the user)
    return originalCost.subs(constSubs);
}

InfiniteInstances::Result InfiniteInstances::check(const ITRSProblem &its, GuardList guard, Expression cost, bool isFinalCheck) {
    Timing::Scope timer(Timing::Infinity);
    assert(GuardToolbox::isValidGuard(guard));

    //abort if there is no model at all
    auto z3res = Z3Toolbox::checkExpressionsSAT(guard);
    if (z3res == z3::unsat) return Result(Expression::ComplexNone,"unsat");

    //if cost is INF, a single model for the guard is sufficient
    if (cost.isInfty() && z3res == z3::sat) return Result(Expression::ComplexInfty,false,Expression::Infty,0,"INF sat");

    //abort if cost is trivial
    debugInfinity("COST: " << cost);
    if (cost.getVariables().empty()) return Result(0,false,cost,0,"const cost");

    //if cost contains infty, check if coefficient > 0 is SAT, otherwise remove infty symbol
    if (cost.has(Expression::Infty)) {
        Expression inftyCoeff = cost.coeff(Expression::Infty);
        guard.push_back(inftyCoeff > 0);
        if (Z3Toolbox::checkExpressionsSAT(guard) == z3::sat) return Result(Expression::ComplexInfty,false,Expression::Infty,0,"INF coeff sat");
        guard.pop_back();
        cost = cost.subs(Expression::Infty == 0); //remove INF symbol if INF cost cannot be proved
    }

    InfiniteInstances InfIns(its,guard,cost);
    InfIns.generateSymbolMapping();

    InfIns.dumpGuard("input guard");

    //try to eliminate exp terms
    int costExpLevel = InfIns.replaceEXPcost();
    if (InfIns.replaceEXPguard() || costExpLevel > 0) {
        //abort if there is no model at all [try again, guard has changed]
        auto z3res = Z3Toolbox::checkExpressionsSAT(InfIns.guard);
        if (z3res == z3::unsat) return Result(Expression::ComplexNone,"unsat");
    }
    InfIns.dumpGuard("noEXP guard");

    //guard and cost must be polynomial for this check
    if (!GuardToolbox::isPolynomialGuard(InfIns.guard,its.getGinacVarList())) return Result(Expression::ComplexNone,"non-polynomial guard");
    if (!InfIns.cost.is_polynomial(its.getGinacVarList())) return Result(Expression::ComplexNone,"non-polynomial cost");

    //eliminate all equalities
    InfIns.removeEqualitiesFromGuard();
    InfIns.dumpGuard("inequality guard");

    if (InfIns.cost.getVariables().empty()) return Result(0,false,InfIns.cost,0,"const cost");

    InfIns.makePolynomialGuard();
    if (!InfIns.removeTrivialFromGuard()) return Result(0,"trivial unsat");

    InfIns.dumpGuard("final polynomial guard");

    InfIns.generatePolynomData();
    InfIns.dumpPolynoms();

    //start the process
    set<InftyCfg> configs = { InfIns.getInitialConfig() };
    InfIns.dumpConfigs(configs);
    InfIns.applyPolynomsToConfigs(configs);

    CheckResult res = InfIns.calcTotalComplexity(configs,costExpLevel,isFinalCheck);
    Expression finalCost(0);
    if (res.cpx >= 0) {
        if (isFinalCheck) {
            finalCost = InfIns.buildProofBound(res.constSubs);
            if (!InfIns.nonlinearSubs.empty()) proofout << "  Applied nonlinear substitutions: " << InfIns.nonlinearSubs << endl;
            proofout << "  Found configuration with infinitely models for cost: " << InfIns.originalCost << endl;
            proofout << "  and guard: ";
            for (int i=0; i < guard.size(); ++i) {
                if (i > 0) proofout << " && ";
                proofout << guard[i];
            }
            proofout << ":" << endl << "  ";
            InfIns.printCfg(res.cfg,proofout);
            proofout << endl << endl;
        }
        return Result(res.cpx,res.reducedCpx,finalCost,res.inftyVars,"Found infinity configuration");
    }
    return Result(Expression::ComplexNone,"All const/invalid");
}
