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
#include "itrs.h"
#include "expression.h"

#include "debug.h"
#include "stats.h"
#include "timing.h"
#include "timeout.h"

#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <vector>
#include <iomanip>
#include <ginac/ginac.h>
#include <z3++.h>


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


bool FlowGraph::simplifyTransitions() {
    Timing::Scope _timer(Timing::Preprocess);
    //remove unreachable transitions/nodes
    bool changed = removeConstLeafsAndUnreachable();
    //update/guard preprocessing
    for (TransIndex idx : getAllTrans()) {
        if (Timeout::preprocessing()) return changed;
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

bool FlowGraph::removeSelfloops() {
    Timing::Scope timer(Timing::Selfloops);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("FlowGraph::removeSelfloops");

    bool res = false;
    for (NodeIndex node : nodes) {
        if (!getTransFromTo(node,node).empty()) {
            res = tryInstantiate(node) || res;
            res = removeSelfloopsFrom(node) || res; //note: inserting while iterating is ok for std::set
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


bool FlowGraph::compareTransitions(TransIndex ia, TransIndex ib) const {
    const Transition &a = getTransData(ia);
    const Transition &b = getTransData(ib);
    if (a.guard.size() != b.guard.size()) return false;
    if (a.update.size() != b.update.size()) return false;
    if (!GiNaC::is_a<GiNaC::numeric>(a.cost-b.cost)) return false; //cost equal up to constants
    for (const auto &itA : a.update) {
        auto itB = b.update.find(itA.first);
        if (itB == b.update.end()) return false;
        if (!itB->second.is_equal(itA.second)) return false;
    }
    for (int i=0; i < a.guard.size(); ++i) {
        if (!a.guard[i].is_equal(b.guard[i])) return false;
    }
    return true;
}


bool FlowGraph::removeDuplicateTransitions(const std::vector<TransIndex> &trans) {
    bool changed = false;
    for (int i=0; i < trans.size(); ++i) {
        for (int j=i+1; j < trans.size(); ++j) {
            if (compareTransitions(trans[i],trans[j])) {
                proofout << "Removing duplicate transition: " << trans[i] << "." << endl;
                removeTrans(trans[i]);
                changed = true;
                goto nextouter; //do not remove trans[i] again
            }
        }
nextouter:;
    }
    return changed;
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
        auto checkRes = InfiniteInstances::check(itrs,getTransData(trans).guard,getTransData(trans).cost,true);
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
            if (cpx == Expression::ComplexInfty) break;
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


bool FlowGraph::removeSelfloopsFrom(NodeIndex node) {
    vector<TransIndex> loops = getTransFromTo(node,node);
    if (loops.empty()) return true;
    proofout << "Eliminating " << loops.size() << " self-loops for location ";
    if (node < itrs.getTermCount()) proofout << itrs.getTerm(node).name; else proofout << "[" << node << "]";
    proofout << endl;
    debugGraph("Eliminating " << loops.size() << " selfloops for node: " << node);

    //split node into incoming and outgoing node, where loop will be replaced by single transition between them
    NodeIndex outNode = addNode();
    splitNode(node,outNode);

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
    bool add_empty = false;
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
            addTrans(node,outNode,getTransData(tidx));
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
                TransIndex newIdx = addTrans(node,outNode,data);
                proofout << "  Self-Loop " << tidx << " has unbounded runtime, resulting in the new transition " << newIdx << "." << endl;
            }
            else if (result == FarkasMeterGenerator::Nonlinear) {
                Stats::add(Stats::SelfloopNoRank);
                debugGraph("Farkas nonlinear!");
                add_empty = true;
                addTrans(node,outNode,getTransData(tidx)); //keep old
            }
            else if (result == FarkasMeterGenerator::Unsat) {
                Stats::add(Stats::SelfloopNoRank);
                debugGraph("Farkas unsat!");
                add_empty = true;
                added_unranked.insert(addTrans(node,outNode,data)); //keep old, mark as unsat
            }
            else if (result == FarkasMeterGenerator::Success) {
                debugGraph("RANK: " << rankfunc);
                if (!Recurrence::calcIterated(itrs,data,rankfunc)) {
                    //do not add to added_unranked, as this will probably not help with nested loops
                    Stats::add(Stats::SelfloopNoUpdate);
                    add_empty = true;
                    addTrans(node,outNode,getTransData(tidx)); //keep old
                } else {
                    Stats::add(Stats::SelfloopRanked);
                    TransIndex tnew = addTrans(node,outNode,data);
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
                        TransIndex tnew = addTrans(node,outNode,loop);
                        added_nested.insert(tnew);
                        proofout << tnew;

                        //try one outer iteration first as well (this is costly, but nested loops are often quadratic!)
                        Transition pre = getTransData(outer);
                        if (chainTransitionData(pre,loop)) {
                            TransIndex tnew = addTrans(node,outNode,pre);
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
                        TransIndex tnew = addTrans(node,outNode,loop);
                        added_nested.insert(tnew);
                        proofout << tnew;

                        //try one inner iteration first as well (this is costly, but nested loops are often quadratic!)
                        Transition pre = getTransData(inner);
                        if (chainTransitionData(pre,loop)) {
                            TransIndex tnew = addTrans(node,outNode,pre);
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
                    TransIndex newtrans = addTrans(node,outNode,chained);
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

#ifdef SELFLOOP_ALLOW_ZEROEXEC
    {
#else
    if (add_empty) {
#endif
        //ALSO add the choice to *not* take a loop at all (in case of unsats only)
        Transition noloop;
        noloop.cost = Expression(0);
        TransIndex tnew = addTrans(node,outNode,noloop);
        proofout << "Adding an epsilon transition (to model nonexecution of the loops): " << tnew << "." << endl;
    }

    removeDuplicateTransitions(getTransFromTo(node,outNode));

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

                    auto res = InfiniteInstances::check(itrs,data.guard,data.cost,false);
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

            auto checkRes = InfiniteInstances::check(itrs,data.guard,data.cost,true);
            if (checkRes.cpx > res.cpx) {
                proofout << "Found new complexity " << Expression::complexityString(checkRes.cpx) << ", because: " << checkRes.reason << "." << endl << endl;
                res.cpx = checkRes.cpx;
                res.bound = checkRes.cost;
                res.reducedCpx = checkRes.reducedCpx;
                res.guard = data.guard;
                if (res.cpx == Expression::ComplexInfty) goto done;
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

/**********************************************************/

// New functions developed in 2017

z3::expr FlowGraph::ginacToZ3(GiNaC::ex ex, z3::context &c) {
    if (GiNaC::is_a<GiNaC::add>(ex)) {
        z3::expr res = ginacToZ3(ex.op(0), c);
        for (int i=1; i < (int)ex.nops(); ++i) {
            res = res + ginacToZ3(ex.op(i), c);
        }
        return res;
    }
    else if (GiNaC::is_a<GiNaC::mul>(ex)) {
        z3::expr res = ginacToZ3(ex.op(0), c);
        for (int i=1; i < (int)ex.nops(); ++i) {
            res = res * ginacToZ3(ex.op(i), c);
        }
        return res;
    }
    else if (GiNaC::is_a<GiNaC::power>(ex)) {
        if (GiNaC::is_a<GiNaC::numeric>(ex.op(1))) {
            // rewrite power as multiplication if possible, which z3 can handle much better
            GiNaC::numeric num = GiNaC::ex_to<GiNaC::numeric>(ex.op(1));
            if (num.is_integer() and num.is_positive() and num.to_int() <= 5) {
                int exp = num.to_int();
                z3::expr base = ginacToZ3(ex.op(0), c);
                z3::expr res = base;
                while (--exp > 0) res = res * base;
                return res;
            }
        }
        // use z3 power as fallback (only poorly supported)
        return z3::pw(ginacToZ3(ex.op(0), c), ginacToZ3(ex.op(1), c));
    }
    else if (GiNaC::is_a<GiNaC::numeric>(ex)) {
        GiNaC::numeric num = GiNaC::ex_to<GiNaC::numeric>(ex);
        if (num.denom().to_int() == 1) {
            return c.int_val(num.numer().to_int());
        }
        else {
            z3::expr numer = c.int_val(num.numer().to_int());
            z3::expr denom = c.int_val(num.denom().to_int());
            return numer / denom;
        }
    }
    else if (GiNaC::is_a<GiNaC::symbol>(ex)) {
        GiNaC::symbol symbol = GiNaC::ex_to<GiNaC::symbol>(ex);
        return c.int_const(symbol.get_name().c_str());
    }
    else if (GiNaC::is_a<GiNaC::relational>(ex)) {
        z3::expr a = ginacToZ3(ex.op(0), c);
        z3::expr b = ginacToZ3(ex.op(1), c);
        if (ex.info(GiNaC::info_flags::relation_equal)) return a == b;
        if (ex.info(GiNaC::info_flags::relation_not_equal)) return (a <= b) && (a >= b);
        if (ex.info(GiNaC::info_flags::relation_less)) return a <= b - 1;
        if (ex.info(GiNaC::info_flags::relation_less_or_equal)) return a <= b;
        if (ex.info(GiNaC::info_flags::relation_greater)) return a >= b + 1;
        if (ex.info(GiNaC::info_flags::relation_greater_or_equal)) return a >= b;
    }
    else std::cerr << "Cannot convert from GiNaC::ex to z3::expr" << std::endl;
    return c.int_val(0);
}

z3::expr FlowGraph::ginacToZ3(GuardList guard, z3::context &c, std::set<VariableIndex> protectedFreeVariables = std::set<VariableIndex>()) {
    z3::goal g1(c);
    for (GiNaC::ex ex : guard) {
        g1.add(ginacToZ3(ex, c));
    }
    z3::expr_vector freeVariables(c);
    std::set<VariableIndex> s = itrs.getFreeVars();
    for (VariableIndex idx : s) {
        if (protectedFreeVariables.count(idx) == 0) {
            freeVariables.push_back(c.int_const(itrs.getVarname(idx).c_str()));
        }
    }
    z3::goal g2(c);
    g2.add(z3::exists(freeVariables, g1.as_expr()));
    z3::tactic t1(c, "ctx-solver-simplify");
    z3::tactic t2(c, "qe");
    z3::apply_result res = (t1 & t2)(g2);
    g2 = res[0];
    return g2.as_expr();
}

std::list<std::string> tokenize(std::string str) {
    std::list<std::string> tokens;
    const char * s = str.c_str();
    while (*s) {
        while (*s == ' ' || *s == (char)10)
            ++s;
        if (*s == '(' || *s == ')')
            tokens.push_back(*s++ == '(' ? "(" : ")");
        else {
            const char * t = s;
            while (*t && *t != ' ' && *t != '(' && *t != ')')
                ++t;
            tokens.push_back(std::string(s, t));
            s = t;
        }
    }
    return tokens;
}

GiNaC::ex FlowGraph::read_from(std::list<std::string> &tokens) {
    std::string token(tokens.front());
    tokens.pop_front();
    if (token == "(") {
        GiNaC::ex res;
        std::string op = tokens.front();
        tokens.pop_front();
        if (op == "=") {
            GiNaC::ex a = read_from(tokens);
            GiNaC::ex b = read_from(tokens);
            res = GiNaC::ex(a == b);
        }
        else if (op == ">") {
            GiNaC::ex a = read_from(tokens);
            GiNaC::ex b = read_from(tokens);
            res = GiNaC::ex(a > b);
        }
        else if (op == "<") {
            GiNaC::ex a = read_from(tokens);
            GiNaC::ex b = read_from(tokens);
            res = GiNaC::ex(a < b);
        }
        else if (op == ">=") {
            GiNaC::ex a = read_from(tokens);
            GiNaC::ex b = read_from(tokens);
            res = GiNaC::ex(a >= b);
        }
        else if (op == "<=") {
            GiNaC::ex a = read_from(tokens);
            GiNaC::ex b = read_from(tokens);
            res = GiNaC::ex(a <= b);
        }
        else if (op == "+") {
            res = GiNaC::ex(0);
            while (tokens.front() != ")") {
                res += read_from(tokens);
            }
        }
        else if (op == "*") {
            res = GiNaC::ex(1);
            while (tokens.front() != ")") {
                res *= read_from(tokens);
            }
        }
        else if (op == "-") {
            res = read_from(tokens);
            if (tokens.front() == ")") res = -res;
            else res -= read_from(tokens);
        }
        else if (op == "/") {
            GiNaC::ex a = read_from(tokens);
            GiNaC::ex b = read_from(tokens);
            res = a / b;
        }
        else if (op == "pow") {
            GiNaC::ex a = read_from(tokens);
            GiNaC::ex b = read_from(tokens);
            res = GiNaC::pow(a, b);
        }
        else {
            std::cerr << "Cannot convert from z3::expr to GiNaC::ex " << op << std::endl;
            for (std::string s : tokens) std::cerr << s;
            std::cerr << std::endl;
        }
        tokens.pop_front();
        return res;
    }
    else {
        if (token.size() == 0)
            return GiNaC::ex(0);
        else if (token == "true")
            return GiNaC::ex(0);
        else if (token == "false")
            return GiNaC::ex(0);
        else if (isdigit(token[0]) || (token[0] == '-' && isdigit(token[1])))
            return GiNaC::ex(std::stoi(token));
        else  return itrs.getGinacSymbol(itrs.getVarindex(token));
    }
}

GiNaC::ex FlowGraph::z3ToGinac(z3::expr expr) {
    std::stringstream ss;
    ss << expr;
    std::string str = ss.str();
    std::list<std::string> tokens = tokenize(str);
    return read_from(tokens);
}

GuardList FlowGraph::z3ToGuard(z3::expr expr) {
    std::stringstream ss;
    ss << expr;
    std::string str = ss.str();
    std::list<std::string> tokens = tokenize(str);
    GuardList res;
    if (tokens.front() == "(") {
        tokens.pop_front();
        if (tokens.front() == "and") {
            tokens.pop_front();
            while (tokens.front() != ")") {
                res.push_back(Expression(read_from(tokens)));
            }
            return res;
        }
        else {
            res.push_back(Expression(z3ToGinac(expr)));
            return res;
        }
    }
    else {
        res.push_back(Expression(z3ToGinac(expr)));
        return res;
    }
}

/**********************************************************/

bool FlowGraph::tryInstantiate(NodeIndex node) {

    // Guess and Check

    std::vector<TransIndex> vLoops = getTransFromTo(node, node);
    std::stack<Transition> stackTransitions;
    std::vector<Transition> v1Transitions;  // Transitions before acceleration
    std::vector<Transition> v2Transitions;  // Transitions after acceleration
    // Put all loops into a stack
    for (TransIndex idx : vLoops) {
        stackTransitions.push(getTransData(idx));
    }

    // Now we instantiate all updates like X=X+Y and X=X-Y with Y=1 and Y=-1
    // The resulting transitions will be stored in v1Transitions
    while (!stackTransitions.empty()) {
        Transition trans = stackTransitions.top();
        stackTransitions.pop();
        UpdateMap update = trans.update;
        for (auto iter : update) {
            // For each update item, check if it is like X=X+?
            ExprSymbol X = itrs.getGinacSymbol(iter.first);
            Expression ex = iter.second;
            ExprSymbolSet symbolSet = ex.getVariables();
            if (symbolSet.count(X) > 0) {  // ex contains X
                ex = ex - X;  // Try removing X
                symbolSet = ex.getVariables();
                if (symbolSet.count(X) > 0) {  // X is not removed
                    v2Transitions.push_back(trans);
                    goto Next1;
                }
                else {  // X is removed
                    if (GiNaC::is_a<GiNaC::numeric>(ex));  // X=X+1 or X=X-1
                    else if (GiNaC::is_a<GiNaC::symbol>(ex) or GiNaC::is_a<GiNaC::symbol>(-ex)) {
                        // X=X+Y or X=X-Y
                        GiNaC::symbol Y;
                        if (GiNaC::is_a<GiNaC::symbol>(ex)) {
                            Y = GiNaC::ex_to<GiNaC::symbol>(ex);
                        }
                        else {  // GiNaC::is_a<GiNaC::symbol>(-ex)
                            Y = GiNaC::ex_to<GiNaC::symbol>(-ex);
                        }

                        // Check if Y is updated
                        for (auto it : update) {
                            if (itrs.getGinacSymbol(it.first) == Y) {
                                // If Y is updated, give up
                                v2Transitions.push_back(trans);
                                goto Next1;
                            }
                        }

                        // Instantiate Y with 1
                        GiNaC::exmap exmap1;
                        exmap1[Y] = GiNaC::ex(1);
                        Transition newTrans1 = trans;
                        for (auto &it : newTrans1.update) {  // Instantiate update
                            it.second = it.second.subs(exmap1);
                        }
                        for (auto &it : newTrans1.guard) {  // Instantiate guard
                            it = it.subs(exmap1);
                        }
                        newTrans1.guard.push_back(Y==1);
                        newTrans1.cost = newTrans1.cost.subs(exmap1);
                        stackTransitions.push(newTrans1);

                        // Instantiate Y with -1
                        GiNaC::exmap exmap2;
                        exmap2[Y] = GiNaC::ex(-1);
                        Transition newTrans2 = trans;
                        for (auto &it : newTrans2.update) {  // Instantiate update
                            it.second = it.second.subs(exmap2);
                        }
                        for (auto &it : newTrans2.guard) {  // Instantiate guard
                            it = it.subs(exmap2);
                        }
                        newTrans2.guard.push_back(Y==-1);
                        newTrans2.cost = newTrans2.cost.subs(exmap2);
                        stackTransitions.push(newTrans2);

                        goto Next1;
                    }
                    else {
                        // Can not be handled
                        v2Transitions.push_back(trans);
                        goto Next1;
                    }
                }
            }
            else {  // ex does not contain X

            }
        }
        v1Transitions.push_back(trans);
        Next1: ;
    }

    // Remove original transitions
    for (TransIndex idx : vLoops) {
        removeTrans(idx);
    }

    // Now we have instantiated all updates like X=X+Y and X=X-Y with Y=1 and Y=-1
    // The resulting transitions are stored in v1Transitions

    // A structrue to describe the update
    // Let k be the number of iterations
    // X = start + k * step
    // step = 0 means X is a constant
    struct BasicUpdate {
        Expression start;
        Expression step;
    };

    // Next we accelerate each transition in v1Transitions and put the result into v2Transitions
    for (Transition &trans : v1Transitions) {
        std::map<VariableIndex, BasicUpdate> m;
        // Transform updates into BasicUpdates
        while (m.size() < trans.update.size()) {
            bool changed = false;
            for (auto iter : trans.update) {
                GiNaC::symbol k("k");
                VariableIndex idx = iter.first;
                if (m.count(idx) > 0) {  // already visited
                    continue;
                }
                Expression ex = iter.second;
                ExprSymbolSet symbols = ex.getVariables();
                GiNaC::exmap exmap;
                for (ExprSymbol symbol : symbols) {
                    VariableIndex index = itrs.getVarindex(symbol.get_name());
                    if (index == idx) {  // X=X+1 or X=X-1
                        BasicUpdate newBU;
                        newBU.start = symbol;
                        newBU.step = ex - symbol;
                        m[idx] = newBU;
                        changed = true;
                        goto Next2;
                    }
                    else if (itrs.isFreeVar(index)) {  // ex contains free variables
                        BasicUpdate newBU;
                        newBU.start = ex;
                        newBU.step = Expression(0);
                        m[idx] = newBU;
                        changed = true;
                        goto Next2;
                    }
                    else if (trans.update.count(index) > 0 and m.count(index) == 0) {  // symbol is dependent on another variable
                        goto Next2;  // try it later
                    }
                    // symbol is already visited
                    BasicUpdate bU = m[index];
                    exmap[symbol] = bU.start + bU.step * k;
                }
                // Now we have finished constructing exmap
                // Transform exmap to newBU
                {
                    ex = ex.subs(exmap);
                    BasicUpdate newBU;
                    newBU.step = ex.coeff(k);
                    if (!(GiNaC::is_a<GiNaC::numeric>(newBU.step))) {  // not a constant
                        // Can not handle this transition
                        v2Transitions.push_back(trans);
                        goto Next3;
                    }
                    // newBU.step is constant
                    newBU.start = ex - newBU.step * k;
                    ExprSymbolSet s = newBU.start.getVariables();
                    if (s.count(k) > 0) {
                        // k is not linear in ex
                        // Can not handle this transition
                        v2Transitions.push_back(trans);
                        goto Next3;
                    }
                    m[idx] = newBU;
                    changed = true;
                    //std::cerr << "newBU:  start = " << newBU.start << ",  step = " << newBU.step << std::endl;
                }
                Next2: ;
            }
            if (!changed) {
                v2Transitions.push_back(trans);
                goto Next3;
            }
        }

        // Now we have finished constructing m

        /*for (auto iter : m) {
            //std::map<VariableIndex, BasicUpdate> m;
            BasicUpdate BU = iter.second;
            std::cerr << itrs.getVarname(iter.first) << "   ";
            std::cerr << "start = " << BU.start << "   ";
            std::cerr << "step = " << BU.step << std::endl;
        }
        std::cerr << std::endl;*/

        // Next we analyze the guard
        {
            // Prepare protectedFreeVariables
            std::set<VariableIndex> protectedFreeVariables;
            for (auto iter : m) {
                BasicUpdate BU = iter.second;
                for (std::string variableName : BU.start.getVariableNames()) {
                    protectedFreeVariables.insert(itrs.getVarindex(variableName));
                }
            }

            // handle equation
            int n = trans.guard.size();
            for (int i = 0; i < n; ++i) {
                Expression ex = trans.guard[i];
                if (GiNaC::is_a<GiNaC::relational>(ex) and ex.info(GiNaC::info_flags::relation_equal)) {
                    trans.guard[i] = ex.op(0) <= ex.op(1);
                    trans.guard.push_back(ex.op(0) >= ex.op(1));
                }
            }

            // Simplify guard
            z3::context c;
            z3::expr expr = ginacToZ3(trans.guard, c, protectedFreeVariables);
            std::stringstream ss;
            ss << expr;
            if (ss.str() == "true") {
                GuardList emptyGuard;
                trans.guard = emptyGuard;
            }
            else if (ss.str() == "false") {
                // guard can not be satisfied
                goto Next3;
            }
            else {
                trans.guard = z3ToGuard(expr);

            }
            auto isUpperBound = [&](VariableIndex idx, Expression &ex) {
                if (GiNaC::is_a<GiNaC::relational>(ex) == false) {
                    return false;
                }
                if (ex.info(GiNaC::info_flags::relation_less_or_equal) or ex.info(GiNaC::info_flags::relation_greater_or_equal)) {
                    GiNaC::ex bound;
                    GiNaC::symbol X = itrs.getGinacSymbol(idx);
                    if (ex.info(GiNaC::info_flags::relation_less_or_equal)) {
                        bound = ex.op(1) - ex.op(0) + X;
                    }
                    else {  // ex.info(GiNaC::info_flags::relation_greater_or_equal)
                        bound = ex.op(0) - ex.op(1) + X;
                    }
                    if (bound.coeff(X) == GiNaC::ex(0)) {  // bound does not contain X
                        ex = X <= bound;
                        return true;
                    }
                    return false;
                }
                return false;
            };

            auto isLowerBound = [&](VariableIndex idx, Expression &ex) {
                if (GiNaC::is_a<GiNaC::relational>(ex) == false) {
                    return false;
                }
                if (ex.info(GiNaC::info_flags::relation_less_or_equal) or ex.info(GiNaC::info_flags::relation_greater_or_equal)) {
                    GiNaC::ex bound;
                    GiNaC::symbol X = itrs.getGinacSymbol(idx);
                    if (ex.info(GiNaC::info_flags::relation_less_or_equal)) {
                        bound = ex.op(0) - ex.op(1) + X;
                    }
                    else {  // ex.info(GiNaC::info_flags::relation_greater_or_equal)
                        bound = ex.op(1) - ex.op(0) + X;
                    }
                    if (bound.coeff(X) == GiNaC::ex(0)) {  // bound does not contain X
                        ex = X >= bound;
                        return true;
                    }
                    return false;
                }
                return false;
            };

            auto getUpperBound = [&](VariableIndex idx, Expression &ex) {
                // Must call isUpperBound first!
                return ex.op(1);
            };

            auto getLowerBound = [&](VariableIndex idx, Expression &ex) {
                // Must call isLowerBound first!
                return ex.op(1);
            };

            // Analyze upper bound or lower bound
            bool noBound = true;
            for (Expression ex : trans.guard) {
                //std::cerr << "ex = " << ex << std::endl;

                for (auto iter : m) {
                    BasicUpdate bU = iter.second;

                    // Prepare newTrans
                    Transition newTrans;

                    int step = GiNaC::ex_to<GiNaC::numeric>(bU.step).to_int();
                    Expression k;

                    if (step > 0 && isUpperBound(iter.first, ex)) {
                        // Increasing, find upper bound
                        if (step == 1) {
                            k = getUpperBound(iter.first, ex) - bU.start + 1;
                        }
                        else {
                            k = itrs.getGinacSymbol(itrs.addFreshVariable("meter"));
                            newTrans.guard.push_back(k * step == getUpperBound(iter.first, ex) - bU.start + step);
                            //newTrans.guard.push_back(k * step > getUpperBound(iter.first, ex) - bU.start);
                        }
                    }
                    else if (step < 0 && isLowerBound(iter.first, ex)) {
                        // Decreasing, find lower bound
                        if (step == -1) {
                            k = bU.start - getLowerBound(iter.first, ex) + 1;
                        }
                        else {
                            k = itrs.getGinacSymbol(itrs.addFreshVariable("meter"));
                            newTrans.guard.push_back(k * (-step) == bU.start - getLowerBound(iter.first, ex) - step);
                            //newTrans.guard.push_back(k * (-step) > bU.start - getLowerBound(iter.first, ex));
                        }
                    }
                    else continue;  // step == 0

                    Recurrence recurrence(itrs);

                    // Calculate iterated update
                    recurrence.calcIteratedUpdate(trans.update, k, newTrans.update);

                    // Calculate iterated cost
                    recurrence.calcIteratedCost(trans.cost, k, newTrans.cost);

                    // Calculate iterated guard
                    Recurrence recurrenceKMinusOne(itrs);
                    UpdateMap kMinusOne;
                    recurrenceKMinusOne.calcIteratedUpdate(trans.update, k - 1, kMinusOne);
                    // Build substitution map for k-1
                    GiNaC::exmap exmap;
                    for (auto it : kMinusOne) {
                        exmap[itrs.getGinacSymbol(it.first)] = it.second;
                    }
                    // Add guard at iteration 0
                    for (Expression guardEx : trans.guard) {
                        newTrans.guard.push_back(guardEx);
                    }
                    // Add guard at iteration k-1
                    for (Expression guardEx : trans.guard) {
                        newTrans.guard.push_back(guardEx.subs(exmap).expand());
                    }
                    // Simplify guard
                    z3::context c;

                    //std::cerr << std::endl << "GUARD 1" << std::endl;
                    //for (Expression ex : newTrans.guard) std::cerr << ex << " && ";

                    z3::expr expr = ginacToZ3(newTrans.guard, c, protectedFreeVariables);
                    std::stringstream ss;
                    ss << expr;
                    if (ss.str() == "true");
                    else if (ss.str() == "false") {
                        // guard can not be satisfied
                        goto Next3;
                    }
                    else newTrans.guard = z3ToGuard(expr);

                    //std::cerr << std::endl << "GUARD 2" << std::endl;
                    //for (Expression ex : newTrans.guard) std::cerr << ex << " && ";

                    // Push newTrans into v2Transitions
                    v2Transitions.push_back(newTrans);
                    noBound = false;
                }
            }
            if (noBound) {
                Transition newTrans;

                // Build substitution map for iteration 0 and 1
                Recurrence recurrenceOne(itrs);
                UpdateMap One;
                recurrenceOne.calcIteratedUpdate(trans.update, Expression(1), One);
                GiNaC::exmap exmap;
                for (auto it : One) {
                    exmap[itrs.getGinacSymbol(it.first)] = it.second;
                }
                // Add guard at iteration 0
                newTrans.guard = trans.guard;
                // Add guard at iteration 1
                for (Expression guardEx : trans.guard) {
                    newTrans.guard.push_back(guardEx.subs(exmap).expand());
                }
                // Simplify guard
                z3::context c;
                z3::expr expr = ginacToZ3(newTrans.guard, c, protectedFreeVariables);
                std::stringstream  ss;
                ss << expr;
                if (ss.str() == "true");
                else if (ss.str() == "false") {
                    // guard can not be satisfied
                    goto Next3;
                }
                else newTrans.guard = z3ToGuard(expr);
                newTrans.cost = Expression::Infty;
                v2Transitions.push_back(newTrans);
            }
        }
        Next3: ;
    }

    // Now the accelerated transitions are stored in v2Transitions
    // Add them back to the graph
    for (Transition &trans : v2Transitions) {
        addTrans(node, node, trans);
    }

    // Some output
    proofout << std::endl << "Try instantiation" << std::endl;
    printForProof();
    return true;
}
