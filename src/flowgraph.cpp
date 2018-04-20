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

#include "flowgraph.h"

#include "preprocess.h"
#include "recurrence.h"
#include "z3toolbox.h"
#include "farkas.h"
#include "infinity.h"
#include "asymptotic/asymptoticbound.h"

#include "debug.h"
#include "stats.h"
#include "timing.h"
#include "timeout.h"

#include <queue>
#include <iomanip>
#include <purrs.hh>


namespace Purrs = Parma_Recurrence_Relation_Solver;
using namespace std;


ostream& operator<<(ostream &s, const Transition &trans) {
    s << "Transition(";
    for (auto upit : trans.update) {
        s << upit.first << "=" << upit.second;
        s << ", ";
    }
    s << "| ";
    for (auto expr : trans.guard) {
        s << expr << ", ";
    }
    s << "| ";
    s << trans.cost;
    s << ")";
    return s;
}




FlowGraph::FlowGraph(ITRSProblem &itrs)
    : itrs(itrs)
{
    TermIndex i;
    for (i=0; i < itrs.getTermCount(); ++i) {
        nodes.insert((NodeIndex)i);
    }
    nextNode = i;
    initial = itrs.getStartTerm();

    for (const Rule &r : itrs.getRules()) {
        addRule(r);
    }

    if (!getPredecessors(initial).empty()) {
        debugGraph("the start location has incoming transitions, adding new start location");
        NodeIndex newStart = addNode();
        Transition epsilon;
        epsilon.cost = Expression(0);
        addTrans(newStart, initial, epsilon);
        initial = newStart;
    }
}


void FlowGraph::addRule(const Rule &rule) {
    Transition trans;
    NodeIndex src = (NodeIndex) rule.lhsTerm;
    NodeIndex dst = (NodeIndex) rule.rhsTerm;
    trans.guard = rule.guard;
    trans.cost = rule.cost;

    const Term &targetTerm = itrs.getTerm(rule.rhsTerm);
    assert(targetTerm.args.size() == rule.rhsArgs.size());

    for (int i=0; i < targetTerm.args.size(); ++i) {
        Expression update = rule.rhsArgs[i];
        VariableIndex var = targetTerm.args[i];
        //avoid adding nontrivial updates (i.e. x=x)
        if (!update.equalsVariable(itrs.getGinacSymbol(var)))
            trans.update[var] = update;
    }

    addTrans(src,dst,std::move(trans));
}


NodeIndex FlowGraph::addNode() {
    nodes.insert(nextNode);
    return nextNode++;
}


bool FlowGraph::isEmpty() const {
    return getTransFrom(initial).empty();
}


bool FlowGraph::preprocessTransitions(bool eliminateCostConstraints) {
    Timing::Scope _timer(Timing::Preprocess);
    //remove unreachable transitions/nodes
    bool changed = removeConstLeafsAndUnreachable();
    //update/guard preprocessing
    for (TransIndex idx : getAllTrans()) {
        if (Timeout::preprocessing()) return changed;
        if (eliminateCostConstraints) {
            changed = Preprocess::tryToRemoveCost(itrs,getTransData(idx).guard) || changed;
        }
        changed = Preprocess::simplifyTransition(itrs,getTransData(idx)) || changed;
    }
    //remove duplicates
    for (NodeIndex node : nodes) {
        for (NodeIndex succ : getSuccessors(node)) {
            if (Timeout::preprocessing()) return changed;
            changed = removeDuplicateTransitions(getTransFromTo(node,succ)) || changed;
        }
    }
    return changed;
}


bool FlowGraph::reduceInitialTransitions() {
    bool changed = false;
    for (TransIndex trans : getTransFrom(initial)) {
        if (Z3Toolbox::checkExpressionsSAT(getTransData(trans).guard) == z3::unsat) {
            removeTrans(trans);
            changed = true;
        }
    }
    return changed;
}


bool FlowGraph::removeDuplicateInitialTransitions() {
    //remove duplicates and ignore the update for the comparison
    return removeDuplicateTransitions(getTransFrom(initial),false);
}


bool FlowGraph::chainLinear() {
    Timing::Scope timer(Timing::Contract);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("FlowGraph::chainLinear");

    set<NodeIndex> visited;
    bool res = chainLinearPaths(initial,visited);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER CONTRACT ===========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER CONTRACT ===========/ " << endl;
#endif
    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool FlowGraph::eliminateALocation() {
    Timing::Scope timer(Timing::Contract);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("FlowGraph::eliminateALocation");

    set<NodeIndex> visited;
    bool res = eliminateALocation(initial, visited);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER ELIMINATING LOCATIONS ===========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER ELIMINATING LOCATIONS ===========/ " << endl;
#endif
    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool FlowGraph::chainBranches() {
    Timing::Scope timer(Timing::Branches);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("FlowGraph::chainBranches");

    set<NodeIndex> visited;
    bool res = chainBranchedPaths(initial,visited);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER BRANCH CONTRACT ===========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER BRANCH CONTRACT ===========/ " << endl;
#endif
    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool FlowGraph::chainSimpleLoops() {
    Timing::Scope timer(Timing::Contract);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("FlowGraph::chainSimpleLoops");

    bool res = false;
    for (NodeIndex node : nodes) {
        if (!getTransFromTo(node, node).empty()) {
            if (chainSimpleLoops(node)) {
                res = true;
            }

            if (Timeout::soft()) return res;
        }
    }

#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER CHAINING SIMPLE LOOPS ===========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER CHAINING SIMPLE LOOPS ===========/ " << endl;
#endif
    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool FlowGraph::accelerateSimpleLoops() {
    Timing::Scope timer(Timing::Selfloops);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("FlowGraph::accelerateSimpleLoops");

    addTransitionToSkipLoops.clear();
    bool res = false;
    for (NodeIndex node : nodes) {
        if (!getTransFromTo(node,node).empty()) {
            res = backwardAccelerateSimpleLoops(node) || res;
            if (Timeout::soft()) return res;
        }
    }
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER SELFLOOPS ==========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER SELFLOOPS ==========/ " << endl;
#endif

    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool FlowGraph::compareTransitions(TransIndex ia, TransIndex ib, bool compareUpdate) const {
    const Transition &a = getTransData(ia);
    const Transition &b = getTransData(ib);
    if (a.guard.size() != b.guard.size()) return false;
    if (compareUpdate && a.update.size() != b.update.size()) return false;
    if (!GiNaC::is_a<GiNaC::numeric>(a.cost-b.cost)) return false; //cost equal up to constants

    if (compareUpdate) {
        for (const auto &itA : a.update) {
            auto itB = b.update.find(itA.first);
            if (itB == b.update.end()) return false;
            if (!itB->second.is_equal(itA.second)) return false;
        }
    }
    for (int i=0; i < a.guard.size(); ++i) {
        if (!a.guard[i].is_equal(b.guard[i])) return false;
    }
    return true;
}


bool FlowGraph::removeDuplicateTransitions(const std::vector<TransIndex> &trans, bool compareUpdate) {
    set<TransIndex> toRemove;
    for (int i=0; i < trans.size(); ++i) {
        for (int j=i+1; j < trans.size(); ++j) {
            if (compareTransitions(trans[i],trans[j],compareUpdate)) {
                //transitions identical up to cost, keep the one with the higher cost (worst case)
                Expression ci = getTransData(trans[i]).cost;
                Expression cj = getTransData(trans[j]).cost;
                if (GiNaC::ex_to<GiNaC::numeric>(ci-cj).is_positive()) {
                    toRemove.insert(j);
                } else {
                    toRemove.insert(i);
                    goto nextouter; //do not remove trans[i] again
                }
            }
        }
nextouter:;
    }
    for (TransIndex idx : toRemove) {
        proofout << "Removing duplicate transition: " << trans[idx] << "." << endl;
        removeTrans(trans[idx]);
    }
    return !toRemove.empty();
}


bool FlowGraph::isFullyChained() const {
    //ensure that all transitions start from initial node
    for (NodeIndex node : nodes) {
        if (node == initial) continue;
        if (!getTransFrom(node).empty()) return false;
    }
    return true;
}


RuntimeResult FlowGraph::getMaxRuntime() {
    auto vec = getTransFrom(initial);

    proofout << "Computing complexity for remaining " << vec.size() << " transitions." << endl << endl;

#ifdef DEBUG_PROBLEMS
    Complexity oldMaxCpx = Expression::ComplexNone;
    Expression oldMaxExpr(0);
#endif

    Complexity cpx;
    RuntimeResult res;

    for (TransIndex trans : vec) {
#ifdef FINAL_INFINITY_CHECK

        Complexity oldCpx = getTransData(trans).cost.getComplexity();

#ifdef DEBUG_PROBLEMS
        if (oldCpx > oldMaxCpx) {
            oldMaxCpx = oldCpx;
            oldMaxExpr = getTransData(trans).cost;
        }
#endif
        //avoid infinity checks that cannot improve the result
        if (oldCpx <= res.cpx) continue;

        //check if this transition allows infinitely many guards
        debugGraph(endl << "INFINITY CHECK");
        auto checkRes = AsymptoticBound::determineComplexity(itrs, getTransData(trans).guard, getTransData(trans).cost, true);
        debugGraph("RES: " << checkRes.cpx << " because: " << checkRes.reason);
        if (checkRes.cpx == Expression::ComplexNone) {
            debugGraph("INFINITY: FAIL");
            continue;
        }
        debugGraph("INFINITY: Success!");
        cpx = checkRes.cpx;
#else //FINAL_INFINITY_CHECK
        cpx = getTransData(trans).cost.getComplexity();
#endif
        if (cpx > res.cpx) {
            res.cpx = cpx;
#ifdef FINAL_INFINITY_CHECK
            proofout << "Found new complexity " << Expression::complexityString(cpx) << ", because: " << checkRes.reason << "." << endl << endl;
            res.bound = checkRes.cost;
            res.reducedCpx = checkRes.reducedCpx;
            res.guard = getTransData(trans).guard;
#endif
            if (cpx == Expression::ComplexInfty || cpx == Expression::ComplexNonterm) break;
        }

        if (Timeout::hard()) return res;
    }

#ifdef DEBUG_PROBLEMS
    if (oldMaxCpx > res.cpx) {
        debugProblem("Infinity lost complexity: " << oldMaxCpx << " [" << oldMaxExpr << "] --> " << res.cpx << " [" << res.bound << "]");
    }
#endif

    return res;
}


bool FlowGraph::chainTransitionData(Transition &trans, const Transition &followTrans) const {
    //build update replacement list
    GiNaC::exmap updateSubs;
    for (auto it : trans.update) {
        updateSubs[itrs.getGinacSymbol(it.first)] = it.second;
    }

    //build new guard and check if it is SAT before continuing
    GuardList newGuard = trans.guard;
    for (const Expression &ex : followTrans.guard) {
        newGuard.push_back(ex.subs(updateSubs));
    }
    Expression newCost = trans.cost + followTrans.cost.subs(updateSubs);

#ifdef CONTRACT_CHECK_SAT
    auto z3res = Z3Toolbox::checkExpressionsSAT(newGuard);

#ifdef CONTRACT_CHECK_SAT_APPROXIMATE
    //try to solve an approximate problem instead, as we do not need 100% soundness here
    if (z3res == z3::unknown) {
        debugProblem("Contract unknown, try approximation for: " << trans << " + " << followTrans);
        z3res = Z3Toolbox::checkExpressionsSATapproximate(newGuard);
    }
#endif

#ifdef CONTRACT_CHECK_EXP_OVER_UNKNOWN
    if (z3res == z3::unknown && newCost.getComplexity() == Expression::ComplexExp) {
        debugGraph("Contract: keeping unknown because of EXP cost");
        z3res = z3::sat;
    }
#endif

    if (z3res != z3::sat) {
        debugGraph("Contract: aborting due to UNSAT for transitions: " << trans << " + " << followTrans);
        Stats::add(Stats::ContractUnsat);
#ifdef DEBUG_PROBLEMS
        if (z3res == z3::unknown) {
            debugProblem("Contract final unknown for: " << trans << " + " << followTrans);
        }
#endif
        return false;
    }
#endif

    trans.guard = std::move(newGuard);

    //modify update (in two steps to allow trans, followTrans to point to the same data)
    UpdateMap newUpdates;
    for (auto it : followTrans.update) newUpdates[it.first] = it.second.subs(updateSubs);
    for (auto it : newUpdates) trans.update[it.first] = std::move(it.second);

    //add up cost, but keep INF if present
    if (trans.cost.isInfty() || followTrans.cost.isInfty()) {
        trans.cost = Expression::Infty;
    } else {
        trans.cost = std::move(newCost);
    }
    return true;
}


bool FlowGraph::chainLinearPaths(NodeIndex node, set<NodeIndex> &visited) {
    if (visited.count(node) > 0) return false;

    bool modified = false;
    bool changed;
    do {
        changed = false;
        vector<TransIndex> out = getTransFrom(node);
        for (TransIndex t : out) {
            NodeIndex dst = getTransTarget(t);
            if (dst == initial) continue; //avoid isolating the initial node (has an implicit "incoming edge")

            //check for a safe linear path, i.e. dst has no other incoming and outgoing transitions
            vector<TransIndex> dstOut = getTransFrom(dst);
            set<NodeIndex> dstPred = getPredecessors(dst);
            if (dstOut.size() == 1 && dstPred.size() == 1 && getTransFromTo(*dstPred.begin(),dst).size() == 1) {
                if (chainTransitionData(getTransData(t),getTransData(dstOut[0]))) {
                    changeTransTarget(t,getTransTarget(dstOut[0]));
                    removeNode(dst);
                    nodes.erase(dst);
                    changed = true;
                    Stats::add(Stats::ContractLinear);
                }
            }
        }
        modified = changed || modified;
        if (Timeout::soft()) return modified;
    } while (changed);

    visited.insert(node);
    for (NodeIndex next : getSuccessors(node)) {
        modified = chainLinearPaths(next,visited) || modified;
        if (Timeout::soft()) return modified;
    }
    return modified;
}


bool FlowGraph::eliminateALocation(NodeIndex node, set<NodeIndex> &visited) {
    if (visited.count(node) > 0) return false;
    visited.insert(node);

    debugGraph("trying to eliminate location " << node);

    set<NodeIndex> predecessors = std::move(getPredecessors(node));

    vector<TransIndex> transitionsIn;
    for (NodeIndex pre : predecessors) {
        for (TransIndex transition : getTransFromTo(pre, node)) {
            transitionsIn.push_back(transition);
        }
    }

    vector<TransIndex> transitionsOut = std::move(getTransFrom(node));

    set<NodeIndex> nextNodes;
    if (predecessors.count(node) > 0 // simple loop
        || transitionsIn.empty()
        || transitionsOut.empty()) {

        for (NodeIndex next : getSuccessors(node)) {
            if (eliminateALocation(next, visited)) {
                return true;
            }

            if (Timeout::soft()) {
                return false;
            }
        }

        return false;
    }

    assert(node != initial);

    bool addedTrans = false;
    for (TransIndex out : transitionsOut) {
        const Transition &outTransData = getTransData(out);

        for (TransIndex in : transitionsIn) {
            Transition inTransData = getTransData(in);

            if (chainTransitionData(inTransData, outTransData)) {
                addedTrans = true;
                addTrans(getTransSource(in), getTransTarget(out), inTransData);
                Stats::add(Stats::ContractLinear);
            }
        }
    }

    for (TransIndex trans : transitionsOut) {
        nextNodes.insert(getTransTarget(trans));
        removeTrans(trans);
    }

    if (addedTrans) {
        for (TransIndex trans : transitionsIn) {
            removeTrans(trans);
        }

        removeNode(node);
        nodes.erase(node);
    }

    return true;
}


bool FlowGraph::chainBranchedPaths(NodeIndex node, set<NodeIndex> &visited) {
    //avoid cycles even in branched mode. Contract a cycle to a selfloop and stop.
    if (visited.count(node) > 0) return false;

    bool modified = false;
    bool changed;
    do {
        changed = false;
        vector<TransIndex> out = getTransFrom(node);
        for (TransIndex t : out) {
            NodeIndex mid = getTransTarget(t);

            //check if skipping mid is sound, i.e. it is not a selfloop and has no other predecessors
            if (mid == node) continue; //ignore selfloops
            assert(getPredecessors(mid).count(node) == 1);
            if (getPredecessors(mid).size() > 1) continue; //this is a "V" pattern, try contracting the rest first [node = loop head]

            //now contract with all children of mid, to "skip" mid
            vector<TransIndex> midout = getTransFrom(mid);
            if (midout.empty()) continue;

            for (TransIndex t2 : midout) {
                assert (mid != getTransTarget(t2)); //selfloops cannot occur ("V" check above)
                if (Timeout::soft()) break;

                Transition data = getTransData(t);
                if (chainTransitionData(data,getTransData(t2))) {
                    addTrans(node,getTransTarget(t2),data);
                    Stats::add(Stats::ContractBranch);
                } else {
                    //if UNSAT, add a new dummy node to keep the first part of the transition (which is removed below)
                    if (data.cost.getComplexity() > 0) {
                        NodeIndex dummy = addNode();
                        addTrans(node,dummy,data); //keep at least first transition
                    }
                }
            }

            removeTrans(t);
            changed = true;
            if (Timeout::soft()) break;
        }
        modified = modified || changed;
        if (Timeout::soft()) return modified;
    } while (changed);

    //this node cannot be contracted further, try its children
    visited.insert(node);
    for (NodeIndex next : getSuccessors(node)) {
        modified = chainBranchedPaths(next,visited) || modified;
        if (Timeout::soft()) return modified;
    }

    //only for the main caller, reduce unreachable stuff
    if (node == initial) {
        removeConstLeafsAndUnreachable();
    }

    return modified;
}


bool FlowGraph::canNest(const Transition &inner, const Transition &outer) const {
    set<string> innerguard;
    for (const Expression &ex : inner.guard) ex.collectVariableNames(innerguard);
    for (const auto &it : outer.update) {
        if (innerguard.count(itrs.getVarname(it.first)) > 0) {
            return true;
        }
    }
    return false;
}


bool FlowGraph::chainSimpleLoops(NodeIndex node) {
    debugGraph("Chaining simple loops.");
    assert(node != initial);
    assert(!getTransFromTo(node, node).empty());

    set<NodeIndex> predecessors = std::move(getPredecessors(node));
    predecessors.erase(node);

    // the bool marks whether this transition was sucessfully chained
    // with a simple loop
    vector<std::pair<TransIndex,bool>> transitions;
    for (NodeIndex pre : predecessors) {
        for (TransIndex transition : getTransFromTo(pre, node)) {
            transitions.push_back(std::make_pair(transition, false));
        }
    }
    debugGraph(transitions.size() << " transitions to " << node);

    for (TransIndex simpleLoop : getTransFromTo(node, node)) {
        const Transition &simpleLoopTransData = getTransData(simpleLoop);

        for (std::pair<TransIndex,bool> &pair : transitions) {
            Transition transData = getTransData(pair.first);

            if (chainTransitionData(transData, simpleLoopTransData)) {
                addTrans(getTransSource(pair.first), node, transData);
                Stats::add(Stats::ContractLinear);
                pair.second = true;
            }

        }

        debugGraph("removing simple loop " << simpleLoop);
        removeTrans(simpleLoop);
    }

    for (const std::pair<TransIndex,bool> &pair : transitions) {
        if (pair.second && !(addTransitionToSkipLoops.count(node) > 0)) {
            debugGraph("removing transition " << pair.first);
            removeTrans(pair.first);
        }
    }

    return true;
}

// TODO copied from farkas.cpp -- refactor
std::vector<Expression> reduceGuard(ITRSProblem itrs, Transition t, Z3VariableContext &c) {
    std::vector<Expression> reducedGuard;

    //create Z3 solver here to use push/pop for efficiency
    Z3Solver sol(c);
    for (const Expression &ex : t.guard) sol.add(ex.toZ3(c));

    for (Expression ex : t.guard) {
        bool add = false;
        bool add_always = false;
        GiNaC::exmap updateSubs;
        auto varnames = ex.getVariableNames();
        for (string varname : varnames) {
            VariableIndex vi = itrs.getVarindex(varname);
            //keep constraint if it contains a free variable
            if (itrs.isFreeVar(vi)) {
                add_always = true;
            }
            //keep constraint if it contains an updated variable
            auto upIt = t.update.find(vi);
            if (upIt != t.update.end()) {
                add = true;
                updateSubs[itrs.getGinacSymbol(upIt->first)] = upIt->second;
            }
        }
        //add if ex contains free var OR updated variable and is not trivially true for the update
        if (add_always) {
            reducedGuard.push_back(ex);
        } else if (add) {
            sol.push();
            sol.add(!Expression::ginacToZ3(ex.subs(updateSubs),c));
            bool tautology = (sol.check() == z3::unsat);
            if (!tautology) {
                reducedGuard.push_back(ex);
                sol.pop();
            }
        }
    }
    return reducedGuard;
}

// TODO copied from recurrence.cpp -- refactor
bool findUpdateRecurrence(Expression update, ExprSymbol target, Expression &result) {
    Timing::Scope timer(Timing::Purrs);
    Expression last = Purrs::x(Purrs::Recurrence::n - 1).toGiNaC();
    Purrs::Expr rhs = Purrs::Expr::fromGiNaC(update.subs(target == last));
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(rhs);
        rec.set_initial_conditions({ {1, Purrs::Expr::fromGiNaC(update)} });

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            return false;
        }
        rec.exact_solution(exact);
    } catch (...) {
        //purrs throws a runtime exception if the recurrence is too difficult
        debugPurrs("Purrs failed on x(n) = " << rhs << " with initial x(0)=" << target << " for update " << update);
        return false;
    }

    result = exact.toGiNaC();
    return true;
}

// TODO overlaps with the corresponding method from recurrence.cpp -- refactor
vector<VariableIndex> dependencyOrder(UpdateMap &update, ITRSProblem itrs) {
    vector<VariableIndex> ordering;
    set<VariableIndex> orderedVars;

    while (ordering.size() < update.size()) {
        for (auto up : update) {
            if (orderedVars.count(up.first) > 0) continue;

            //check if all variables on update rhs are already processed
            for (const string &varname : up.second.getVariableNames()) {
                VariableIndex vi = itrs.getVarindex(varname);
                if (vi != up.first && update.count(vi) > 0 && orderedVars.count(vi) == 0)
                    return ordering;
            }

            orderedVars.insert(up.first);
            ordering.push_back(up.first);
        }

    }
    return ordering;
}

bool FlowGraph::backwardAccelerateSimpleLoops(NodeIndex node) {
    vector<TransIndex> loops = getTransFromTo(node,node);
    vector<TransIndex> to_remove;
    vector<Transition> to_add;
    debugBackwardAcceleration("trying to backward-accelerate " << loops.size() << " loops");
    for (int lopidx = 0; lopidx < loops.size(); ++lopidx) {
        if (Timeout::soft()) {
            break;
        }
        TransIndex tidx = loops[lopidx];
        Transition transition = getTransData(tidx);
        if (!transition.cost.is_polynomial(itrs.getGinacVarList()) || !transition.cost.getMaxDegree() == 0) {
            debugBackwardAcceleration("cost function " << transition.cost << " is not constant");
            continue;
        }
        bool linear = true;
        GiNaC::exmap inverse_ginac_update;
        UpdateMap inverse_update;
        for (std::pair<VariableIndex, Expression> p: transition.update) {
            Expression x = itrs.getGinacSymbol(p.first);
            GiNaC::lst xList;
            xList.append(x);
            Expression up = p.second;
            if (!up.isLinear(xList)) {
                linear = false;
                break;
            }
            Expression lincoeff = up.coeff(x, 1);
            Expression in_up = (x / lincoeff) - (up - lincoeff * x) / lincoeff;
            inverse_ginac_update.emplace(x, in_up);
            inverse_update.emplace(p.first, in_up);
        }
        if (!linear) {
            debugBackwardAcceleration("update " << up << " is not linear");
            continue;
        }
        debugBackwardAcceleration("successfully computed inverse update " << inverse_ginac_update);
        // check if the guard implies inverse_update(reduced_guard)
        Z3VariableContext c;
        std::vector<Expression> reduced_guard = reduceGuard(itrs, transition, c);
        z3::expr rhs = c.bool_val(true);
        for (Expression e: reduced_guard) {
            rhs = rhs && Expression::ginacToZ3(e.subs(inverse_ginac_update), c);
        }
        z3::expr lhs = c.bool_val(true);
        for (Expression e: transition.guard) {
            lhs = lhs && Expression::ginacToZ3(e, c);
        }
        Z3Solver solver(c);
        z3::params params(c);
        params.set(":timeout", Z3_CHECK_TIMEOUT);
        solver.set(params);
        solver.add(!rhs && lhs);
        bool tautology = solver.check() == z3::unsat;
        if (tautology) {
            debugBackwardAcceleration("successfully checked guard implication");
            vector<VariableIndex> order = dependencyOrder(inverse_update, itrs);
            if (order.size() == inverse_update.size()) {
                debugBackwardAcceleration("successfully computed dependency order " << order);
                GiNaC::exmap knownPreRecurrences;
                bool successfully_computed_it_update = true;
                for (VariableIndex vi: order) {
                    Expression res;
                    ExprSymbol target = itrs.getGinacSymbol(vi);
                    Expression todo = inverse_ginac_update.at(target).subs(knownPreRecurrences);
                    if (!findUpdateRecurrence(todo, target, res)) {
                        successfully_computed_it_update = false;
                        debugBackwardAcceleration("failed to compute iterated inverse update for " << todo);
                        break;
                    }
                    debugBackwardAcceleration("successfully computed iterated inverse update " << res << " for " << todo);
                    knownPreRecurrences[target] = res;
                }
                if (successfully_computed_it_update) {
                    Transition new_transition;
                    // use a fresh variable to represent the number of iterations to avoid clashes
                    Expression n = itrs.getGinacSymbol(itrs.addFreshVariable("n", true));
                    Expression ginac_n = Purrs::Expr(Purrs::Recurrence::n).toGiNaC();
                    GiNaC::exmap var_map;
                    // TODO here, the "irrelevant guard" would be sufficient
                    new_transition.guard = transition.guard;
                    // the number of iterations needs to be positive
                    new_transition.guard.push_back(n > 0);
                    for (std::pair<VariableIndex, Expression> p: transition.update) {
                        VariableIndex vi = p.first;
                        Expression old_var = itrs.getGinacSymbol(vi);
                        // create a fresh variable which represents the value of the currently processed variable after applying the accelerated transition
                        VariableIndex fresh = itrs.addFreshVariable(itrs.getVarname(vi), true);
                        Expression fresh_var = itrs.getGinacSymbol(fresh);
                        new_transition.update.emplace(vi, fresh_var);
                        // we computed the iterated _inverse_ update, so replace old_var (representing the value before the accelerated transition)
                        // with new_var (representing the value after the accelerated transition)
                        GiNaC::exmap sigma;
                        sigma.emplace(old_var, fresh_var);
                        var_map.emplace(old_var, fresh_var);
                        sigma.emplace(ginac_n, n);
                        // now the value before the transition (old_var) needs to result from the value after the transition by applying the iterated inverse update
                        new_transition.guard.push_back(old_var == knownPreRecurrences.at(old_var).subs(sigma));
                    }
                    // TODO adapt the computation of the iterated costs as soon as we support non-constant costs
                    new_transition.cost = transition.cost * n;
                    // make sure that the transition can be applied in the last step
                    for (Expression e: transition.guard) {
                        Expression final_constraint = e.subs(inverse_ginac_update);
                        if (e != final_constraint) {
                            new_transition.guard.push_back(final_constraint.subs(var_map));
                        }
                    }
                    debugBackwardAcceleration("replacing " << transition " with " << new_transition);
                    to_add.push_back(new_transition);
                    to_remove.push_back(tidx);
                }
            } else {
                debugBackwardAcceleration("failed to computed dependency order");
            }
        } else {
            debugBackwardAcceleration("failed to check guard implication");
        }
    }
    for (TransIndex ti: to_remove) {
        removeTrans(ti);
    }
    for (Transition t: to_add) {
        addTrans(node, node, t);
    }
    return !to_remove.empty();
}

bool FlowGraph::accelerateSimpleLoops(NodeIndex node) {
    vector<TransIndex> loops = getTransFromTo(node,node);
    proofout << "Eliminating " << loops.size() << " self-loops for location ";
    if (node < itrs.getTermCount()) proofout << itrs.getTerm(node).name; else proofout << "[" << node << "]";
    proofout << endl;
    debugGraph("Eliminating " << loops.size() << " selfloops for node: " << node);
    assert(!loops.empty());

    //helper lambda
    auto try_rank = [&](Transition &data) -> bool {
        Expression rankfunc;
        FarkasMeterGenerator::Result res = FarkasMeterGenerator::generate(itrs,data,rankfunc);
        if (res == FarkasMeterGenerator::Unbounded) {
            data.cost = Expression::Infty;
            data.update.clear(); //clear update, but keep guard!
            proofout << "  Found unbounded runtime when nesting loops," << endl;
            return true;
        } else if (res == FarkasMeterGenerator::Success) {
            if (Recurrence::calcIterated(itrs,data,rankfunc)) {
                Stats::add(Stats::SelfloopRanked);
                debugGraph("Farkas nested loop ranked!");
                proofout << "  Found this metering function when nesting loops: " << rankfunc << "," << endl;
                return true;
            }
        }
        return false;
    };

    //first try to find a metering function for every parallel selfloop
    set<TransIndex> added_ranked;
    set<TransIndex> added_unranked;
    set<TransIndex> todo_remove;
    map<TransIndex,TransIndex> map_to_original; //maps ranked transition to the original transition

    //use index to iterate, as loops is appended while iterating
    int oldloopcount = loops.size();
    for (int lopidx=0; lopidx < loops.size(); ++lopidx) {
        if (Timeout::soft()) goto timeout;
        TransIndex tidx = loops[lopidx];

        //remove the original selfloop later
        todo_remove.insert(tidx);

        //abort early on INF selfloops
        if (getTransData(tidx).cost.isInfty()) {
            addTrans(node,node,getTransData(tidx));
            continue;
        }

#ifdef SELFLOOPS_ALWAYS_SIMPLIFY
        Timing::start(Timing::Preprocess);
        if (Preprocess::simplifyTransition(itrs,getTransData(tidx))) {
            debugGraph("Simplified transition before Farkas");
        }
        Timing::done(Timing::Preprocess);
#endif

        Expression rankfunc;
        pair<VariableIndex,VariableIndex> conflictVar;
        Transition data = getTransData(tidx); //note: data possibly modified by instantiation in farkas
        FarkasMeterGenerator::Result result = FarkasMeterGenerator::generate(itrs,data,rankfunc,&conflictVar);

        //this is a second attempt for one selfloop, so ignore it if it was not successful
        if (lopidx >= oldloopcount && result != FarkasMeterGenerator::Unbounded && result != FarkasMeterGenerator::Success) continue;
        if (lopidx >= oldloopcount) debugGraph("MinMax heuristic successful");

#ifdef FARKAS_HEURISTIC_FOR_MINMAX
        if (result == FarkasMeterGenerator::ConflictVar) {
            VariableIndex A,B;
            tie(A,B) = conflictVar;

            //add A > B to the guard, process resulting selfloop later
            Transition dataAB = data; //copy
            dataAB.guard.push_back(itrs.getGinacSymbol(A) > itrs.getGinacSymbol(B));
            loops.push_back(addTrans(node,node,dataAB));

            //add B > A to the guard, process resulting selfloop later
            Transition dataBA = data; //copy
            dataBA.guard.push_back(itrs.getGinacSymbol(B) > itrs.getGinacSymbol(A));
            loops.push_back(addTrans(node,node,dataBA));

            //ConflictVar is really just Unsat
            result = FarkasMeterGenerator::Unsat;
        }
#endif

        for (int step=0; step < 2; ++step) {
            if (result == FarkasMeterGenerator::Unbounded) {
                Stats::add(Stats::SelfloopInfinite);
                debugGraph("Farkas unbounded!");
                data.cost = Expression::Infty;
                data.update.clear(); //clear update, but keep guard!
                TransIndex newIdx = addTrans(node,node,data);
                proofout << "  Self-Loop " << tidx << " has unbounded runtime, resulting in the new transition " << newIdx << "." << endl;
            }
            else if (result == FarkasMeterGenerator::Nonlinear) {
                Stats::add(Stats::SelfloopNoRank);
                debugGraph("Farkas nonlinear!");
                addTransitionToSkipLoops.insert(node);
                addTrans(node,node,getTransData(tidx)); //keep old
            }
            else if (result == FarkasMeterGenerator::Unsat) {
                Stats::add(Stats::SelfloopNoRank);
                debugGraph("Farkas unsat!");
                addTransitionToSkipLoops.insert(node);
                added_unranked.insert(addTrans(node,node,data)); //keep old, mark as unsat
            }
            else if (result == FarkasMeterGenerator::Success) {
                debugGraph("RANK: " << rankfunc);
                if (!Recurrence::calcIterated(itrs,data,rankfunc)) {
                    //do not add to added_unranked, as this will probably not help with nested loops
                    Stats::add(Stats::SelfloopNoUpdate);
                    addTransitionToSkipLoops.insert(node);
                    addTrans(node,node,getTransData(tidx)); //keep old
                } else {
                    Stats::add(Stats::SelfloopRanked);
                    TransIndex tnew = addTrans(node,node,data);
                    added_ranked.insert(tnew);
                    added_unranked.insert(tidx); //try nesting also with original transition
                    map_to_original[tnew] = tidx;
                    proofout << "  Self-Loop " << tidx << " has the metering function: " << rankfunc << ", resulting in the new transition " << tnew << "." << endl;
                }
            }

            if (result != FarkasMeterGenerator::Unsat) break;

#ifdef FARKAS_TRY_ADDITIONAL_GUARD
            if (step >= 1) break;
            //try again after adding helpful constraints to the guard
            if (FarkasMeterGenerator::prepareGuard(itrs,data)) {
                debugGraph("Farkas unsat try again after prepareGuard");
                result = FarkasMeterGenerator::generate(itrs,data,rankfunc);
            }
            if (result != FarkasMeterGenerator::Success && result != FarkasMeterGenerator::Unbounded) break;
            //if this was successful, the original transition is still marked as unsat (for nested loops!)
#else
            break;
#endif
        }
    }

    //try nesting loops (inner loop ranked, outer loop not [or perhaps it is?])
    for (int i=0; i < NESTING_MAX_ITERATIONS; ++i) {
        debugGraph("Nesting iteration: " << i);
        bool changed = false;
        set<TransIndex> added_nested;
        for (TransIndex inner : added_ranked) {
            for (TransIndex outer : added_unranked) {
                if (Timeout::soft()) goto timeout;

                //dont nest a loop with itself or with its original transition (save time)
                if (inner == outer) continue;
                if (map_to_original[inner] == outer) continue;

                //dont nest if the inner loop has constant runtime
                if (getTransData(inner).cost.getComplexity() == 0) continue;

                //check if we can nest at all
                if (!canNest(getTransData(inner),getTransData(outer))) continue;

                //inner loop first
                Transition loop = getTransData(inner);
                if (chainTransitionData(loop,getTransData(outer))) {
                    Complexity oldcpx = loop.cost.getComplexity();
                    if (try_rank(loop) && loop.cost.getComplexity() > oldcpx) {
                        proofout << "  and nested parallel self-loops " << outer << " (outer loop) and " << inner << " (inner loop), obtaining the new transitions: ";
                        changed = true;
                        todo_remove.insert(outer); //remove the previously unsat loop
                        TransIndex tnew = addTrans(node,node,loop);
                        added_nested.insert(tnew);
                        proofout << tnew;

                        //try one outer iteration first as well (this is costly, but nested loops are often quadratic!)
                        Transition pre = getTransData(outer);
                        if (chainTransitionData(pre,loop)) {
                            TransIndex tnew = addTrans(node,node,pre);
                            added_nested.insert(tnew);
                            proofout << ", " << tnew;
                        }
                        proofout << "." << endl;
                    }
                }

                //outer loop first
                loop = getTransData(outer);
                if (chainTransitionData(loop,getTransData(inner))) {
                    Complexity oldcpx = loop.cost.getComplexity();
                    if (try_rank(loop) && loop.cost.getComplexity() > oldcpx) {
                        proofout << "  and nested parallel self-loops " << outer << " (outer loop) and " << inner << " (inner loop), obtaining the new transitions: ";
                        changed = true;
                        todo_remove.insert(outer); //remove the previously unsat loop
                        TransIndex tnew = addTrans(node,node,loop);
                        added_nested.insert(tnew);
                        proofout << tnew;

                        //try one inner iteration first as well (this is costly, but nested loops are often quadratic!)
                        Transition pre = getTransData(inner);
                        if (chainTransitionData(pre,loop)) {
                            TransIndex tnew = addTrans(node,node,pre);
                            added_nested.insert(tnew);
                            proofout << ", " << tnew;
                        }
                        proofout << "." << endl;
                    }
                }
            }
        }

        debugGraph("Nested loops: " << added_nested.size());

#ifdef NESTING_CHAIN_RANKED
        for (TransIndex first : added_ranked) {
            for (TransIndex second : added_ranked) {
                if (Timeout::soft()) goto timeout;
                if (first == second) continue;
                Transition chained = getTransData(first);
                if (chainTransitionData(chained,getTransData(second))) {
                    TransIndex newtrans = addTrans(node,node,chained);
                    added_nested.insert(newtrans);
                    changed = true;
                    proofout << "  Chained the parallel self-loops " << first << " and " << second << ", obtaining the new transition: " << newtrans << "." << endl;
                }
            }
        }
        debugGraph("Nested+chained loops: " << added_nested.size());
#endif

        if (!changed) break;
        added_ranked.swap(added_nested); //remove ranked loops, try to nest nested loops one more time
    }

timeout:
    //remove unsat transitions that were used in nested loops
    proofout << "  Removing the self-loops:";
    for (TransIndex tidx : todo_remove) {
        proofout << " " << tidx;
        removeTrans(tidx);
    }
    proofout << "." << endl;

    removeDuplicateTransitions(getTransFromTo(node,node));

    return true; //always changed as old transition is removed
}



bool FlowGraph::pruneTransitions() {
    bool changed = removeConstLeafsAndUnreachable();

#ifndef PRUNING_ENABLE
    return false;
#else
    Stats::addStep("Flowgraph::pruneTransitions");
    typedef tuple<TransIndex,Complexity,int> TransCpx;
    auto comp = [](TransCpx a, TransCpx b) { return get<1>(a) < get<1>(b) || (get<1>(a) == get<1>(b) && get<2>(a) < get<2>(b)); };

    for (NodeIndex node : nodes) {
        if (Timeout::soft()) break;
        for (NodeIndex pre : getPredecessors(node)) {
            const vector<TransIndex> &parallel = getTransFromTo(pre,node);

            if (parallel.size() > PRUNE_MAX_PARALLEL_TRANSITIONS) {
                priority_queue<TransCpx,std::vector<TransCpx>,decltype(comp)> queue(comp);

                for (int i=0; i < parallel.size(); ++i) {
                    //alternating iteration (front,end) that might avoid choosing similar edges
                    int idx = (i % 2 == 0) ? i/2 : parallel.size()-1-i/2;
                    TransIndex trans = parallel[idx];
                    Transition data = getTransData(trans);

                    auto res = AsymptoticBound::determineComplexity(itrs, getTransData(trans).guard, getTransData(trans).cost, false);
                    queue.push(make_tuple(trans,res.cpx,res.inftyVars));
                }

                set<TransIndex> keep;
                for (int i=0; i < PRUNE_MAX_PARALLEL_TRANSITIONS; ++i) {
                    keep.insert(get<0>(queue.top()));
                    queue.pop();
                }

                bool has_empty = false;
                for (TransIndex trans : parallel) {
                    const Transition &data = getTransData(trans);
                    if (!has_empty && data.update.empty() && data.guard.empty() && data.cost.is_zero()) {
                        has_empty = true;
                    } else {
                        if (keep.count(trans) == 0) {
                            Stats::add(Stats::PruneRemove);
                            removeTrans(trans);
                        }
                    }
                }
                changed = true;
            }
        }
    }
    return changed;

#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER PRUNING ==========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER PRUNING ==========/ " << endl;
#endif
#endif
}


bool FlowGraph::removeConstLeafsAndUnreachable() {
    bool changed = false;
    set<NodeIndex> reached;
    function<void(NodeIndex)> dfs_remove;
    dfs_remove = [&](NodeIndex curr) {
        if (reached.insert(curr).second == false) return; //already present
        for (NodeIndex next : getSuccessors(curr)) {
            //recurse
            dfs_remove(next);

            //if next is (now) a leaf, remove const transitions to next
            if (!getTransFrom(next).empty()) continue;
            for (TransIndex trans : getTransFromTo(curr,next)) {
                if (getTransData(trans).cost.getComplexity() <= 0) {
                    removeTrans(trans);
                    changed = true;
                }
            }
        }
    };
    dfs_remove(initial);

    //remove nodes not seen on dfs
    for (auto it = nodes.begin(); it != nodes.end(); ) {
        if (reached.count(*it) == 0) {
            removeNode(*it);
            it = nodes.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    return changed;
}



/* ### Recovering after timeout ### */

bool FlowGraph::removeIrrelevantTransitions(NodeIndex curr, set<NodeIndex> &visited) {
    if (visited.insert(curr).second == false) return true; //already seen, remove any transitions forminig a loop

    for (NodeIndex next : getSuccessors(curr)) {
        if (Timeout::hard()) return false;
        if (removeIrrelevantTransitions(next,visited)) {
            //only const costs below next, so keep only non-const transitions to next
            auto checktrans = getTransFromTo(curr,next);
            for (TransIndex trans : checktrans) {
                if (getTransData(trans).cost.getComplexity() <= 0) removeTrans(trans);
            }
        }
    }
    return getTransFrom(curr).empty(); //if true, curr is not of any interest anymore
}

RuntimeResult FlowGraph::getMaxPartialResult() {
    //remove all irrelevant transitions to reduce z3 invocations
    set<NodeIndex> visited;
    removeIrrelevantTransitions(initial,visited);
    proofout << "Removed transitions with const cost" << endl;
    printForProof();

    //contract and always compute the maximum complexity to allow abortion
    RuntimeResult res;

    while (true) {
        //always check for timeouts
        if (Timeout::hard()) goto abort;

        //get current max cost (with infinity check)
        for (TransIndex trans : getTransFrom(initial)) {
            const Transition &data = getTransData(trans);
            if (data.cost.getComplexity() <= max(res.cpx,Complexity(0))) continue;

            auto checkRes = AsymptoticBound::determineComplexity(itrs, getTransData(trans).guard, getTransData(trans).cost, true);
            if (checkRes.cpx > res.cpx) {
                proofout << "Found new complexity " << Expression::complexityString(checkRes.cpx) << ", because: " << checkRes.reason << "." << endl << endl;
                res.cpx = checkRes.cpx;
                res.bound = checkRes.cost;
                res.reducedCpx = checkRes.reducedCpx;
                res.guard = data.guard;
                if (res.cpx == Expression::ComplexInfty || res.cpx == Expression::ComplexNonterm) goto done;
            }
            if (Timeout::hard()) goto abort;
        }

        //contract next level (if there is one)
        auto succ = getSuccessors(initial);
        if (succ.empty()) goto done;
        for (NodeIndex mid : succ) {
            for (TransIndex first : getTransFromTo(initial,mid)) {
                for (TransIndex second : getTransFrom(mid)) {
                    Transition data = getTransData(first);
                    if (chainTransitionData(data,getTransData(second))) {
                        addTrans(initial,getTransTarget(second),data);
                    }
                    removeTrans(second);
                    if (Timeout::hard()) goto abort;
                }
                removeTrans(first);
            }
        }
        proofout << "Performed chaining from the start location:" << endl;
        printForProof();
    }
    goto done;
abort:
    proofout << "Aborting due to timeout" << endl;
done:
    return res;
}



/* ### Output ### */

set<VariableIndex> FlowGraph::getBoundVariables(const Transition &trans) const {
    set<VariableIndex> res;
    //updated variables are always bound
    for (const auto &it : trans.update) {
        res.insert(it.first);
    }
    //collect non-free variables from guard and cost
    ExprSymbolSet symbols;
    for (const Expression &ex : trans.guard) {
        ex.collectVariables(symbols);
    }
    trans.cost.collectVariables(symbols);
    for (const ExprSymbol &var : symbols) {
        if (!itrs.isFreeVar(var)) {
            res.insert(itrs.getVarindex(var.get_name()));
        }
    }
    return res;
}

void FlowGraph::print(ostream &s) const {
    auto printVar = [&](VariableIndex vi) {
        s << vi;
        s << "[" << itrs.getVarname(vi) << "]";
    };
    auto printNode = [&](NodeIndex ni) {
        s << ni;
        if (ni < itrs.getTermCount()) s << "[" << itrs.getTerm(ni).name << "]";
    };

    s << "Nodes:";
    for (NodeIndex n : nodes) {
        s << " " << n;
        if (n == initial) s << "*";
    }
    s << endl;

    s << "Transitions:" << endl;
    for (NodeIndex n : nodes) {
        for (TransIndex trans : getTransFrom(n)) {
            printNode(n);
            s << " -> ";
            printNode(getTransTarget(trans));
            const Transition &data = getTransData(trans);
            s << ", update: ";
            for (auto upit : data.update) {
                printVar(upit.first);
                s << "=" << upit.second;
                s << ", ";
            }
            s << "guard: ";
            for (auto expr : data.guard) {
                s << expr << ", ";
            }
            s << "cost: " << data.cost;
            s << endl;
        }
    }
}


void FlowGraph::printForProof() const {
    auto printNode = [&](NodeIndex n) {
        if (n < itrs.getTermCount()) proofout << itrs.getTerm(n).name; else proofout << "[" << n << "]";
    };

    proofout << "  Start location: "; printNode(initial); proofout << endl;
    if (getTransCount() == 0) proofout << "    <empty>" << endl;

    for (NodeIndex n : nodes) {
        for (TransIndex trans : getTransFrom(n)) {
            proofout << "    ";
            proofout << setw(3) << trans << ": ";
            printNode(n);
            proofout << " -> ";
            printNode(getTransTarget(trans));
            proofout << " : ";
            const Transition &data = getTransData(trans);
            for (auto upit : data.update) {
                proofout << itrs.getVarname(upit.first) << "'";
                proofout << "=" << upit.second;
                proofout << ", ";
            }
            if (data.guard.empty()) {
                proofout << "[]";
            } else {
                proofout << "[ ";
                for (int i=0; i < data.guard.size(); ++i) {
                    if (i > 0) proofout << " && ";
                    proofout << data.guard[i];
                }
                proofout << " ]";
            }
            proofout << ", cost: " << data.cost << endl;
        }
    }
    proofout << endl;
}

void FlowGraph::printKoAT() const {
    using namespace GiNaC;
    auto printNode = [&](NodeIndex n) {
        if (n < itrs.getTermCount()) proofout << itrs.getTerm(n).name; else proofout << "loc" << n << "'";
    };

    proofout << "(GOAL COMPLEXITY)" << endl;
    proofout << "(STARTTERM (FUNCTIONSYMBOLS "; printNode(initial); proofout << "))" << endl;
    proofout << "(VAR";
    for (const ex &var : itrs.getGinacVarList()) {
        proofout << " " << ex_to<symbol>(var).get_name();
    }
    proofout << ")" << endl << "(RULES" << endl;

    for (NodeIndex n : nodes) {
        //write transition in KoAT format (note that relevantVars is an ordered set)
        for (TransIndex trans : getTransFrom(n)) {
            const Transition &data = getTransData(trans);
            set<VariableIndex> relevantVars = getBoundVariables(data);

            //lhs
            printNode(n);
            bool first = true;
            for (VariableIndex var : relevantVars) {
                proofout << ((first) ? "(" : ",");
                proofout << itrs.getVarname(var);
                first = false;
            }

            //cost
            proofout << ") -{" << data.cost.expand() << "," << data.cost.expand() << "}> ";

            //rhs update
            printNode(getTransTarget(trans));
            first = true;
            for (VariableIndex var : relevantVars) {
                proofout << ((first) ? "(" : ",");
                auto it = data.update.find(var);
                if (it != data.update.end()) {
                    proofout << it->second.expand();
                } else {
                    proofout << itrs.getVarname(var);
                }
                first = false;
            }

            //rhs guard
            proofout << ") :|: ";
            for (int i=0; i < data.guard.size(); ++i) {
                if (i > 0) proofout << " && ";
                proofout << data.guard[i].expand();
            }
            proofout << endl;
        }
    }
    proofout << ")" << endl;
}


void FlowGraph::printDot(ostream &s, int step, const string &desc) const {
    auto printNodeName = [&](NodeIndex n) { if (n < itrs.getTermCount()) s << itrs.getTerm(n).name; else s << "[" << n << "]"; };
    auto printNode = [&](NodeIndex n) { s << "node_" << step << "_" << n; };

    s << "subgraph cluster_" << step << " {" << endl;
    s << "sortv=" << step << ";" << endl;
    s << "label=\"" << step << ": " << desc << "\";" << endl;
    for (NodeIndex n : nodes) {
        printNode(n); s << " [label=\""; printNodeName(n); s << "\"];" << endl;
    }
    for (NodeIndex n : nodes) {
        for (NodeIndex succ : getSuccessors(n)) {
            printNode(n); s << " -> "; printNode(succ); s << " [label=\"";
            for (TransIndex trans : getTransFromTo(n,succ)) {
                const Transition &data = getTransData(trans);
                for (auto upit : data.update) {
                    s << itrs.getVarname(upit.first) << "=" << upit.second << ", ";
                }
                s << "[";
                for (int i=0; i < data.guard.size(); ++i) {
                    if (i > 0) s << ", ";
                    s << data.guard[i];
                }
                s << "], ";
                s << data.cost.expand(); //simplify for readability
                s << "\\l";
            }
            s << "\"];" << endl;
        }
    }
    s << "}" << endl;
}

void FlowGraph::printDotText(ostream &s, int step, const string &txt) const {
    s << "subgraph cluster_" << step << " {" << endl;
    s << "sortv=" << step << ";" << endl;
    s << "label=\"" << step << ": " << "Result" << "\";" << endl;
    s << "node_" << step << "_result [label=\"" << txt << "\"];" << endl;
    s << "}" << endl;
}

void FlowGraph::printT2(ostream &s) const {
    s << "START: 0;" << endl << endl;
    for (NodeIndex start : nodes) {
        for (TransIndex idx : getTransFrom(start)) {
            const Transition &trans = getTransData(idx);
            s << "FROM: " << start << ";" << endl;
            ExprSymbolSet vars = trans.cost.getVariables();
            for (const Expression &ex : trans.guard) {
                ex.collectVariables(vars);
            }
            for (auto it : trans.update) {
                it.second.collectVariables(vars);
            }

            //create copy of vars ("pre vars") to simulate parallel assignments
            GiNaC::exmap t2subs;
            for (const ExprSymbol &sym : vars) {
                t2subs[sym] = GiNaC::symbol("pre_v" + sym.get_name());
                if (itrs.isFreeVar(itrs.getVarindex(sym.get_name()))) {
                    s << t2subs[sym] << " := nondet();" << endl;
                } else {
                    s << t2subs[sym] << " := v" << sym.get_name() << ";" << endl;
                }
            }

            if (!trans.guard.empty()) {
                s << "assume(";
                for (int i=0; i < trans.guard.size(); ++i) {
                    if (i > 0) s << " && ";
                    s << trans.guard[i].subs(t2subs);
                }
                s << ");" << endl;
            }

            for (auto it : trans.update) {
                s << "v" << itrs.getGinacSymbol(it.first) << " := ";
                s << it.second.subs(t2subs) << ";" << endl;
            }

            s << "TO: " << getTransTarget(idx) << ";" << endl << endl;
        }
    }
}

