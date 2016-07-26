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

#include "recursiongraph.h"

#include <queue>

#include "term.h"
#include "farkas.h"
#include "recursion.h"
#include "preprocessitrs.h"
#include "recurrence.h"
#include "stats.h"
#include "timeout.h"
#include "z3toolbox.h"
#include "asymptotic/asymptoticbound.h"

using namespace std;
namespace Purrs = Parma_Recurrence_Relation_Solver;

const NodeIndex RecursionGraph::NULLNODE = -1;


RightHandSide Transition::toRightHandSide(const ITRSProblem &itrs,
                                                FunctionSymbolIndex funSym) const {
    RightHandSide rhs;
    rhs.cost = TT::Expression(cost);
    for (const Expression &ex : guard) {
        rhs.guard.push_back(TT::Expression(ex));
    }

    std::vector<TT::Expression> args;
    const FunctionSymbol &funSymbol = itrs.getFunctionSymbol(funSym);
    for (VariableIndex var : funSymbol.getArguments()) {
        auto it = update.find(var);

        if (it != update.end()) {
            args.push_back(TT::Expression(it->second));

        } else {
            args.push_back(TT::Expression(itrs.getGinacSymbol(var)));
        }
    }

    TT::Expression call = TT::Expression(funSym, itrs.getFunctionSymbolName(funSym), args);
    TT::Substitution sub;
    sub.emplace(outerUpdateVar, call);
    TT::Expression outer = TT::Expression(outerUpdate);
    rhs.term = outer.unGinacify().substitute(sub).ginacify();

    return rhs;
}


std::ostream& operator<<(std::ostream &s, const Transition &trans) {
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


bool RightHandSide::isLegacyTransition(const ITRSProblem &itrs) const {
    std::set<FunctionSymbolIndex> funSyms = std::move(term.getFunctionSymbols());
    if (funSyms.size() != 1) {
        return false;
    }

    if (cost.hasFunctionSymbol()) {
        return false;
    }

    for (const TT::Expression &ex : guard) {
        if (ex.hasFunctionSymbol()) {
            return false;
        }
    }

    return term.hasExactlyOneFunctionSymbolOnce();
}


Transition RightHandSide::toLegacyTransition(const ITRSProblem &itrs,
                                             const ExprSymbol &outerUpVar,
                                                   FunctionSymbolIndex funSym) const {
    Transition trans;
    assert(cost.hasNoFunctionSymbols());
    trans.cost = cost.toGiNaC();

    for (const TT::Expression &ex : guard) {
        assert(ex.hasNoFunctionSymbols());
        trans.guard.push_back(ex.toGiNaC());
    }

    assert(isLegacyTransition(itrs));
    const FunctionSymbol &funSymbol = itrs.getFunctionSymbol(funSym);
    int arity = funSymbol.getArguments().size();
    assert(arity >= 0);
    std::vector<TT::Expression> updates = std::move(term.getUpdates());
    for (int i = 0; i < arity; ++i) {
        Expression arg = updates[i].toGiNaC();
        VariableIndex varIndex = funSymbol.getArguments()[i];

        if (!arg.is_equal(itrs.getGinacSymbol(varIndex))) {
            trans.update.emplace(varIndex, arg);
        }
    }

    trans.outerUpdate = term.toGiNaC(true, &outerUpVar);
    trans.outerUpdateVar = outerUpVar;
    trans.numberOfCalls = updates.size() / arity;
    assert(updates.size() % arity == 0);

    return trans;
}

void RightHandSide::substitute(const GiNaC::exmap &sub) {
    term = std::move(term.substitute(sub));
    cost = std::move(cost.substitute(sub));
    for (TT::Expression &ex : guard) {
        ex = std::move(ex.substitute(sub));
    }
}

std::ostream& operator<<(std::ostream &os, const RightHandSide &rhs) {
    os << rhs.term << ", [";
    for (int i = 0; i < rhs.guard.size(); ++i) {
        if (i > 0) {
            os << ", ";
        }

        os << rhs.guard[i];
    }
    os << "], " << rhs.cost;

    return os;
}

RecursionGraph::RecursionGraph(ITRSProblem &itrs)
    : itrs(itrs), nextRightHandSide(0), outerUpdateVar("t") {
    nodes.insert(NULLNODE);

    for (FunctionSymbolIndex i = 0; i < itrs.getFunctionSymbolCount(); ++i) {
        nodes.insert((NodeIndex)i);
    }

    initial = itrs.getStartFunctionSymbol();

    for (const Rule &rule : itrs.getRules()) {
        addRule(rule);
    }

    // add a new initial node if the current one has ingoing transitions
    if (!getTransTo(initial).empty()) {
        // create the function symbol initial'
        FunctionSymbolIndex newInitialIndex = itrs.addFunctionSymbolVariant(initial);
        nodes.insert((NodeIndex)newInitialIndex);
        const FunctionSymbol &newInitial = itrs.getFunctionSymbol(newInitialIndex);

        // build the rule initial'(x_1, ..., x_n) -> initial(x_1, ..., x_n)
        std::vector<TT::Expression> vars;
        for (VariableIndex var : newInitial.getArguments()) {
            vars.push_back(TT::Expression(itrs.getGinacSymbol(var)));
        }

        RightHandSide rhs;
        // initial(x_1, ..., x_n)
        rhs.term = TT::Expression(initial, itrs.getFunctionSymbolName(initial), vars);
        // cost 0
        rhs.cost = TT::Expression(GiNaC::numeric(0));

        addRightHandSide(newInitialIndex, std::move(rhs));

        initial = newInitialIndex;
    }
}


bool RecursionGraph::solveRecursion(NodeIndex node) {
    assert(node != NULLNODE);
    debugRecGraph("Solving recursion for " << itrs.getFunctionSymbol(node).getName());

    int funSymbolIndex = (FunctionSymbolIndex)node;

    std::vector<TransIndex> transitions = getTransFrom(node);
    std::set<const RightHandSide*> rhss;
    for (TransIndex index : transitions) {
        rhss.insert(&rightHandSides.at(getTransData(index)));
    }

    std::set<const RightHandSide*> wereUsed;
    std::vector<RightHandSide> result;
    Recursion recursion(itrs, funSymbolIndex, rhss, wereUsed, result);
    if (!recursion.solve()) {
        return false;
    }

    for (TransIndex index : transitions) {
        RightHandSideIndex rhsIndex = getTransData(index);
        const RightHandSide &rhs = rightHandSides.at(rhsIndex);

        if (wereUsed.count(&rhs) == 1) {
            debugRecGraph("rhs " << rhs << " was used for solving the recursion, removing");
            removeRightHandSide(node, rhsIndex);
        }
    }

    debugRecGraph("adding new rhss for the solved recursions");
    for (RightHandSide &rhs : result) {
        addRightHandSide(node, std::move(rhs));
    }

    return true;
}


void RecursionGraph::print(ostream &s) const {
    auto printVar = [&](VariableIndex vi) {
        s << vi;
        s << "[" << itrs.getVariableName(vi) << "]";
    };
    auto printNode = [&](NodeIndex ni) {
        s << ni;
        s << "[";
        if (ni >= 0) {
            itrs.printLhs((FunctionSymbolIndex)ni, s);
        } else {
            s << "null";
        }
        s << "]";
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
            const RightHandSideIndex index = getTransData(trans);
            s << rightHandSides.at(index);
            s << endl;
        }
    }
}


void RecursionGraph::printForProof() const {
    auto printNode = [&](NodeIndex ni) {
        proofout << ni;
        proofout << "[";
        if (ni >= 0) {
            itrs.printLhs((FunctionSymbolIndex)ni, proofout);
        } else {
            proofout << "null";
        }
        proofout << "]";
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
            const RightHandSideIndex index = getTransData(trans);
            proofout << rightHandSides.at(index);
        }
    }
    proofout << endl;
}


void RecursionGraph::printDot(ostream &s, int step, const string &desc) const {
    auto printNodeName = [&](NodeIndex ni) {
        if (ni >= 0) {
            itrs.printLhs((FunctionSymbolIndex)ni, s);
        } else {
            s << "null";
        }
    };
    auto printNode = [&](NodeIndex n) {
        s << "node_" << step << "_";
        if (n >= 0) {
            s << n;
        }
    };

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
                const RightHandSideIndex index = getTransData(trans);
                const RightHandSide &rhs = rightHandSides.at(index);

                s << "(" << index << "): ";
                s << rhs;
                s << "\\l";
            }
            s << "\"];" << endl;
        }
    }
    s << "}" << endl;
}


void RecursionGraph::printDotText(ostream &s, int step, const string &txt) const {
    s << "subgraph cluster_" << step << " {" << endl;
    s << "sortv=" << step << ";" << endl;
    s << "label=\"" << step << ": " << "Result" << "\";" << endl;
    s << "node_" << step << "_result [label=\"" << txt << "\"];" << endl;
    s << "}" << endl;
}


bool RecursionGraph::isEmpty() const {
    return getTransFrom(initial).empty();
}


bool RecursionGraph::simplifyTransitions() {
    Timing::Scope _timer(Timing::Preprocess);
    //remove unreachable transitions/nodes
    bool changed = removeConstLeavesAndUnreachable();
    //update/guard preprocessing
    for (auto &pair : rightHandSides) {
        if (Timeout::preprocessing()) return changed;
        changed = PreprocessITRS::simplifyRightHandSide(itrs, pair.second) || changed;
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


bool RecursionGraph::removeDuplicateTransitions(const std::vector<TransIndex> &trans) {
    bool changed = false;
    for (int i=0; i < trans.size(); ++i) {
        for (int j=i+1; j < trans.size(); ++j) {
            RightHandSideIndex rhsIndexI = getTransData(trans[i]);
            RightHandSideIndex rhsIndexJ = getTransData(trans[j]);
            if (compareRightHandSides(rhsIndexI, rhsIndexJ)) {
                proofout << "Removing duplicate rhs: "
                         << rightHandSides.at(rhsIndexI) << "." << endl;
                removeRightHandSide(getTransSource(trans[i]), rhsIndexI);
                changed = true;
                goto nextouter; //do not remove trans[i] again
            }
        }
nextouter:;
    }
    return changed;
}



bool RecursionGraph::pruneTransitions() {
    bool changed = removeConstLeavesAndUnreachable();

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

            set<TransIndex> keep;
            if (parallel.size() > PRUNE_MAX_PARALLEL_TRANSITIONS) {
                priority_queue<TransCpx,std::vector<TransCpx>,decltype(comp)> queue(comp);

                for (int i=0; i < parallel.size(); ++i) {
                    //alternating iteration (front,end) that might avoid choosing similar edges
                    int idx = (i % 2 == 0) ? i/2 : parallel.size()-1-i/2;
                    TransIndex trans = parallel[idx];

                    RightHandSideIndex rhsIndex = getTransData(trans);
                    const RightHandSide &rhs = rightHandSides.at(rhsIndex);

                    if (!rhs.cost.hasNoFunctionSymbols()) {
                        if (keep.size() < PRUNE_MAX_PARALLEL_TRANSITIONS) {
                            keep.insert(trans);
                        }
                        continue;
                    }
                    Expression cost = rhs.cost.toGiNaC();

                    bool hasFunSymbol = false;
                    std::vector<Expression> guard;
                    for (const TT::Expression &ex : rhs.guard) {
                        if (!ex.hasNoFunctionSymbols()) {
                            hasFunSymbol = true;
                            break;
                        }

                        guard.push_back(ex.toGiNaC());
                    }
                    if (hasFunSymbol) {
                        if (keep.size() < PRUNE_MAX_PARALLEL_TRANSITIONS) {
                            keep.insert(trans);
                        }
                        continue;
                    }

                    auto res = AsymptoticBound::determineComplexity(itrs, guard, cost, false);
                    queue.push(make_tuple(trans,res.cpx,res.inftyVars));
                }

                while (keep.size() < PRUNE_MAX_PARALLEL_TRANSITIONS) {
                    keep.insert(get<0>(queue.top()));
                    queue.pop();
                }

                debugGraph("KEEP IS " << keep.size());

                bool has_empty = false;
                for (TransIndex trans : parallel) {
                    RightHandSideIndex rhsIndex = getTransData(trans);
                    const RightHandSide &rhs = rightHandSides.at(rhsIndex);

                    if (!has_empty && rightHandSideIsEmpty(pre, rhs)) {
                        has_empty = true;
                    } else {
                        if (keep.count(trans) == 0) {
                            Stats::add(Stats::PruneRemove);

                            removeRightHandSide(pre, rhsIndex);
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


bool RecursionGraph::isFullyChained() const {
    //ensure that all transitions start from initial node
    for (NodeIndex node : nodes) {
        if (node == initial) continue;
        if (!getTransFrom(node).empty()) return false;
    }
    return true;
}


RuntimeResult RecursionGraph::getMaxRuntime() {
    std::vector<TransIndex> vec = getTransFrom(initial);

    proofout << "Computing complexity for remaining " << vec.size() << " transitions." << endl << endl;

#ifdef DEBUG_PROBLEMS
    Complexity oldMaxCpx = Expression::ComplexNone;
    Expression oldMaxExpr(0);
#endif

    Complexity cpx;
    RuntimeResult res;

    for (TransIndex trans : vec) {
        const RightHandSide &rhs = rightHandSides.at(getTransData(trans));

        // Make sure that rhs's guard and cost do not contain any function symbols
        bool hasFunSym = false;
        for (const TT::Expression &ex : rhs.guard) {
            if (!ex.hasNoFunctionSymbols()) {
                hasFunSym = true;
                break;
            }
        }
        if (hasFunSym) {
            continue;
        }

        if (!rhs.cost.hasNoFunctionSymbols()) {
            continue;
        }
        // TODO do the same for the parital function
        Expression costGiNaC = rhs.cost.toGiNaC();
        std::vector<Expression> guardGiNaC;
        for (const TT::Expression &ex : rhs.guard) {
            guardGiNaC.push_back(ex.toGiNaC());
        }

        Complexity oldCpx = costGiNaC.getComplexity();

#ifdef DEBUG_PROBLEMS
        if (oldCpx > oldMaxCpx) {
            oldMaxCpx = oldCpx;
            oldMaxExpr = costGiNaC;
        }
#endif
        //avoid infinity checks that cannot improve the result
        bool containsFreeVar = false;
        for (const ExprSymbol &var : costGiNaC.getVariables()) {
            if (itrs.isFreeVariable(var)) {
                // we try to achieve ComplexInfty
                containsFreeVar = true;
                break;
            }
        }
        if (!containsFreeVar && oldCpx <= res.cpx) continue;

        //check if this transition allows infinitely many guards
        debugGraph(endl << "INFINITY CHECK");

        auto checkRes = AsymptoticBound::determineComplexity(itrs, guardGiNaC, costGiNaC, true);
        debugGraph("RES: " << checkRes.cpx << " because: " << checkRes.reason);
        if (checkRes.cpx == Expression::ComplexNone) {
            debugGraph("INFINITY: FAIL");
            continue;
        }
        debugGraph("INFINITY: Success!");
        cpx = checkRes.cpx;

        if (cpx > res.cpx) {
            res.cpx = cpx;

            proofout << "Found new complexity " << Expression::complexityString(cpx) << ", because: " << checkRes.reason << "." << endl << endl;
            res.bound = checkRes.cost;
            res.reducedCpx = checkRes.reducedCpx;
            res.guard = guardGiNaC;

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


RuntimeResult RecursionGraph::getMaxPartialResult() {
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
            const RightHandSide &rhs = rightHandSides.at(getTransData(trans));
            Transition legacy = rhs.toLegacyTransition(itrs, outerUpdateVar, initial);

            if (legacy.cost.getComplexity() <= max(res.cpx,Complexity(0))) continue;

            auto checkRes = AsymptoticBound::determineComplexity(itrs, legacy.guard, legacy.cost, true);
            if (checkRes.cpx > res.cpx) {
                proofout << "Found new complexity " << Expression::complexityString(checkRes.cpx) << ", because: " << checkRes.reason << "." << endl << endl;
                res.cpx = checkRes.cpx;
                res.bound = checkRes.cost;
                res.reducedCpx = checkRes.reducedCpx;
                res.guard = legacy.guard;
                if (res.cpx == Expression::ComplexInfty) goto done;
            }
            if (Timeout::hard()) goto abort;
        }

        //contract next level (if there is one)
        /*auto succ = getSuccessors(initial);
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
        }*/
        // TODO FIX SHIT
        break;
        proofout << "Performed chaining from the start location:" << endl;
        printForProof();
    }
    goto done;
abort:
    proofout << "Aborting due to timeout" << endl;
done:
    return res;
}


bool RecursionGraph::reduceInitialTransitions() {
    bool changed = false;
    for (TransIndex trans : getTransFrom(initial)) {
        RightHandSideIndex rhsIndex = getTransData(trans);
        RightHandSide &rhs = rightHandSides.at(rhsIndex);

        std::vector<Expression> asGiNaC;
        for (const TT::Expression &ex : rhs.guard) {
            // substitute function symbols by variables
            asGiNaC.push_back(ex.toGiNaC(true));
        }

        if (Z3Toolbox::checkExpressionsSAT(asGiNaC) == z3::unsat) {
            removeRightHandSide(initial, rhsIndex);
            changed = true;
        }
    }
    return changed;
}


bool RecursionGraph::chainLinear() {
    Timing::Scope timer(Timing::Contract);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("RecursionGraph::chainLinear");

    set<NodeIndex> visited;
    bool res = chainLinearPaths(initial,visited);
    removeIncorrectTransitionsToNullNode();
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER CONTRACT ===========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER CONTRACT ===========/ " << endl;
#endif
    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool RecursionGraph::eliminateALocation() {
    Timing::Scope timer(Timing::Contract);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("RecursionGraph::eliminateALocation");

    set<NodeIndex> visited;
    debugRecGraph("About to eliminate a location");
    bool res = eliminateALocation(initial, visited);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER ELIMINATING LOCATIONS ===========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER ELIMINATING LOCATIONS ===========/ " << endl;
#endif
    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool RecursionGraph::chainBranches() {
    Timing::Scope timer(Timing::Branches);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("RecursionGraph::chainBranches");

    set<NodeIndex> visited;
    debugRecGraph("About to chain branched paths");
    bool res = chainBranchedPaths(initial,visited);
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER BRANCH CONTRACT ===========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER BRANCH CONTRACT ===========/ " << endl;
#endif
    assert(check(&nodes) == Graph::Valid);
    return res;
}


bool RecursionGraph::chainSimpleLoops() {
    Timing::Scope timer(Timing::Contract);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("RecursionGraph::chainSimpleLoops");

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


bool RecursionGraph::accelerateSimpleLoops() {
    Timing::Scope timer(Timing::Selfloops);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("RecursionGraph::accelerateSimpleLoops");

    addTransitionToSkipLoops.clear();
    bool res = false;
    for (NodeIndex node : nodes) {
        if (!getTransFromTo(node,node).empty()) {
            res = accelerateSimpleLoops(node) || res;
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


bool RecursionGraph::solveRecursions() {
    // TODO Timing
    Timing::Scope timer(Timing::Selfloops);
    assert(check(&nodes) == Graph::Valid);
    Stats::addStep("RecursionGraph::solveRecursions");

    bool res = false;
    for (NodeIndex node : nodes) {
        if (node != NULLNODE) {
            res = solveRecursion(node) || res;
            if (Timeout::soft()) return res;
        }
    }
#ifdef DEBUG_PRINTSTEPS
    cout << " /========== AFTER RECURSIONS ==========\\ " << endl;
    print(cout);
    cout << " \\========== AFTER RECURSIONS ==========/ " << endl;
#endif

    assert(check(&nodes) == Graph::Valid);
    return res;
}


void RecursionGraph::addRule(const Rule &rule) {
    RightHandSide rhs;
    rhs.guard = rule.guard;
    rhs.term = rule.rhs;
    rhs.cost = rule.cost;

    NodeIndex src = (NodeIndex)rule.lhs;
    std::set<NodeIndex> dsts = std::move(getSuccessorsOfRhs(rhs));

    RightHandSideIndex rhsIndex = nextRightHandSide++;
    rightHandSides.emplace(rhsIndex, rhs);

    for (NodeIndex dst : dsts) {
        addTrans(src, dst, rhsIndex);
    }
}


TransIndex RecursionGraph::addLegacyTransition(NodeIndex from,
                                               NodeIndex to,
                                               const Transition &trans) {
    RightHandSide rhs = std::move(trans.toRightHandSide(itrs, to));

    RightHandSideIndex rhsIndex = nextRightHandSide++;
    rightHandSides.emplace(rhsIndex, std::move(rhs)).first;

    return addTrans(from, to, rhsIndex);
}


TransIndex RecursionGraph::addLegacyRightHandSide(NodeIndex from,
                                                  NodeIndex to,
                                                  const RightHandSide &rhs) {
    assert(rhs.term.isSimple());

    RightHandSideIndex rhsIndex = nextRightHandSide++;
    auto it = rightHandSides.emplace(rhsIndex, rhs).first;

    return addTrans(from, to, rhsIndex);
}


RightHandSideIndex RecursionGraph::addRightHandSide(NodeIndex node, const RightHandSide &rhs) {
    RightHandSideIndex rhsIndex = nextRightHandSide++;
    auto it = rightHandSides.emplace(rhsIndex, rhs).first;

    for (NodeIndex to : getSuccessorsOfRhs(it->second)) {
        addTrans(node, to, rhsIndex);
    }

    return rhsIndex;
}


RightHandSideIndex RecursionGraph::addRightHandSide(NodeIndex node, const RightHandSide &&rhs) {
    RightHandSideIndex rhsIndex = nextRightHandSide++;
    auto it = rightHandSides.emplace(rhsIndex, rhs).first;

    for (NodeIndex to : getSuccessorsOfRhs(it->second)) {
        addTrans(node, to, rhsIndex);
    }

    return rhsIndex;
}


bool RecursionGraph::hasExactlyOneRightHandSide(NodeIndex node) {
    std::vector<TransIndex> trans = std::move(getTransFrom(node));
    if (trans.empty()) {
        return false;
    }

    RightHandSideIndex rhsIndex = getTransData(trans[0]);
    for (int i = 1; i < trans.size(); ++i) {
        if (trans[i] != rhsIndex) {
            return false;
        }
    }

    return true;
}


void RecursionGraph::removeRightHandSide(NodeIndex node, RightHandSideIndex rhs) {
    debugGraph("removing rhs #" << rhs << ":");
    if (rightHandSides.count(rhs) == 0) {
        debugGraph("WARNING: NOT PRESENT");

    } else {
        debugGraph(rightHandSides.at(rhs));
    }


    for (TransIndex trans : getTransFrom(node)) {
        if (getTransData(trans) == rhs) {
            removeTrans(trans);
        }
    }
    rightHandSides.erase(rhs);
}


std::set<NodeIndex> RecursionGraph::getSuccessorsOfRhs(const RightHandSide &rhs) {
    std::set<FunctionSymbolIndex> funSyms;

    rhs.term.collectFunctionSymbols(funSyms);
    rhs.cost.collectFunctionSymbols(funSyms);
    for (const TT::Expression &ex : rhs.guard) {
        ex.collectFunctionSymbols(funSyms);
    }

    if (funSyms.empty()) {
        funSyms.insert(NULLNODE);
    }
    return funSyms;
}


bool RecursionGraph::chainRightHandSides(RightHandSide &rhs,
                                         const FunctionSymbolIndex funSymbolIndex,
                                         const RightHandSide &followRhs) const {
    const FunctionSymbol &funSymbol = itrs.getFunctionSymbol(funSymbolIndex);
    const TT::FunctionDefinition funDef(itrs, funSymbolIndex, followRhs.term, followRhs.cost, followRhs.guard);

    debugGraph("About to chain " << rhs);
    debugGraph("with " << itrs.getFunctionSymbolName(funSymbolIndex) << " -> " << followRhs);

    // perform rewriting on a copy of rhs
    RightHandSide rhsCopy(rhs);

    rhsCopy.term = rhsCopy.term.evaluateFunction2(funDef, &rhsCopy.cost, &rhsCopy.guard).ginacify();
    int oldSize = rhsCopy.guard.size();
    for (int i = 0; i < oldSize; ++i) {
           // the following call might add new elements to rhsCopy.guard
           rhsCopy.guard[i] = rhsCopy.guard[i].evaluateFunction2(funDef, &rhsCopy.cost, &rhsCopy.guard).ginacify();
    }
    // We do not evaluate the guard because we avoid moving function symbols there

    GuardList funSymbolFreeGuard;
    for (const TT::Expression &ex : rhsCopy.guard) {
        // toGiNaC(true) -> Substitute function symbols by fresh variables
        funSymbolFreeGuard.push_back(ex.toGiNaC(true));
    }

#ifdef CONTRACT_CHECK_SAT
    auto z3res = Z3Toolbox::checkExpressionsSAT(funSymbolFreeGuard);

#ifdef CONTRACT_CHECK_SAT_APPROXIMATE
    //try to solve an approximate problem instead, as we do not need 100% soundness here
    if (z3res == z3::unknown) {
        debugProblem("Contract unknown, try approximation for: "
                     << rhs << " + " << funSymbol.getName() << " -> " << followRhs);
        z3res = Z3Toolbox::checkExpressionsSATapproximate(funSymbolFreeGuard);
    }
#endif

#ifdef CONTRACT_CHECK_EXP_OVER_UNKNOWN
    Expression funSymbolFreeCost = rhsCopy.cost.toGiNaC(true);
    if (z3res == z3::unknown && funSymbolFreeCost.getComplexity() == Expression::ComplexExp) {
        debugGraph("Contract: keeping unknown because of EXP cost");
        z3res = z3::sat;
    }
#endif

    if (z3res != z3::sat) {
        debugGraph("Contract: aborting due to notSAT for transitions: "
                   << rhs << " + " << funSymbol.getName() << " -> " << followRhs);
        Stats::add(Stats::ContractUnsat);
#ifdef DEBUG_PROBLEMS
        if (z3res == z3::unknown) {
            debugProblem("Contract final unknown for: " << rhs << " + " << followRhs);
        }
#endif
        return false;
    }
#endif

    // move term and guard
    rhs.term = std::move(rhsCopy.term);
    rhs.guard = std::move(rhsCopy.guard);

    // move cost, but keep INF if present
    // TODO optimize (isInfinity() member function)
    if ((rhs.cost.hasNoFunctionSymbols() && Expression(rhs.cost.toGiNaC()).isInfty())
        || (followRhs.cost.hasNoFunctionSymbols() && Expression(followRhs.cost.toGiNaC()).isInfty())) {
        rhs.cost = TT::Expression(Expression::Infty);

    } else {
        rhs.cost = std::move(rhsCopy.cost);
    }

    return true;
}


bool RecursionGraph::chainLinearPaths(NodeIndex node, set<NodeIndex> &visited) {
    if (visited.count(node) > 0) return false;

    bool modified = false;
    bool changed;
    do {
        changed = false;
        vector<TransIndex> out = std::move(getTransFrom(node));
        for (TransIndex t : out) {
            RightHandSideIndex rhsIndex = getTransData(t);
            NodeIndex dst = getTransTarget(t);
            if (dst == initial) continue; //avoid isolating the initial node (has an implicit "incoming edge")

            //check for a safe linear path, i.e. dst has no other incoming and outgoing transitions
            vector<TransIndex> dstOut = getTransFrom(dst);
            if (dstOut.size() > 0) {
                // check if all outgoing transitions are labeled with the same rhs
                RightHandSideIndex followRhsIndex = getTransData(dstOut[0]);
                bool onlyOneRhs = true;
                for (TransIndex index : dstOut) {
                    if (getTransData(index) != followRhsIndex) {
                        onlyOneRhs = false;
                        break;
                    }
                }

                // check if this path is "linear"
                set<NodeIndex> dstPred = std::move(getPredecessors(dst));
                if (onlyOneRhs
                    && dstPred.size() == 1
                    && getTransFromTo(*dstPred.begin(), dst).size() == 1) {
                    RightHandSide &rhs = rightHandSides.at(rhsIndex);
                    const RightHandSide &followRhs = rightHandSides.at(followRhsIndex);

                    if (chainRightHandSides(rhs, dst, followRhs)) {
                        // change the target of t so that one does not have to remove it
                        changeTransTarget(t, getTransTarget(dstOut[0]));
                        // add new for the remaining function symbols
                        for (int i = 1; i < dstOut.size(); i++) {
                            addTrans(*dstPred.begin(), getTransTarget(dstOut[i]), rhsIndex);
                        }
                    }

                    // removing dst also removes all outgoing transitions
                    removeNode(dst);
                    nodes.erase(dst);
                    // remove the chained rhs
                    rightHandSides.erase(followRhsIndex);
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


bool RecursionGraph::eliminateALocation(NodeIndex node, set<NodeIndex> &visited) {
    if (visited.count(node) > 0) return false;
    visited.insert(node);

    debugGraph("trying to eliminate location " << node);


    set<NodeIndex> predecessors = std::move(getPredecessors(node));

    vector<TransIndex> transitionsIn = std::move(getTransTo(node));
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
        RightHandSideIndex outRhsIndex = getTransData(out);
        auto it = rightHandSides.find(outRhsIndex);
        if (it == rightHandSides.end()) {
            // This rhs was already chained
            continue;
        }
        const RightHandSide &outRhs = it->second;

        for (TransIndex in : transitionsIn) {
            RightHandSideIndex inRhsIndex = getTransData(in);
            RightHandSide inRhsCopy = rightHandSides.at(inRhsIndex);

            if (chainRightHandSides(inRhsCopy, node, outRhs)) {
                addedTrans = true;
                addRightHandSide(getTransSource(in), std::move(inRhsCopy));
                Stats::add(Stats::ContractLinear);
            }
        }

        rightHandSides.erase(outRhsIndex);
    }

    for (TransIndex trans : transitionsOut) {
        nextNodes.insert(getTransTarget(trans));
        // the right-hand sides have already been deleted
        removeTrans(trans);
    }

    if (addedTrans) {
        for (TransIndex trans : transitionsIn) {
            RightHandSideIndex rhsIndex = getTransData(trans);
            removeRightHandSide(getTransSource(trans),rhsIndex);
        }

        removeNode(node);
        nodes.erase(node);
    }

    return true;
}


bool RecursionGraph::chainBranchedPaths(NodeIndex node, set<NodeIndex> &visited) {
    //avoid cycles even in branched mode. Contract a cycle to a selfloop and stop.
    if (visited.count(node) > 0) return false;

    bool modified = false;
    bool changed;
    do {
        changed = false;
        vector<TransIndex> out = getTransFrom(node);
        set<RightHandSideIndex> removeThese;
        set<RightHandSideIndex> alreadyHandledThese;
        for (TransIndex t : out) {
            NodeIndex mid = getTransTarget(t);

            //check if skipping mid is sound, i.e. it is not a selfloop and has no other predecessors
            if (mid == node) continue; //ignore selfloops
            assert(getPredecessors(mid).count(node) == 1);
            if (getPredecessors(mid).size() > 1) continue; //this is a "V" pattern, try contracting the rest first [node = loop head]

            //now contract with all children of mid, to "skip" mid
            vector<TransIndex> midout = getTransFrom(mid);
            if (midout.empty()) continue;

            RightHandSideIndex rhsIndex = getTransData(t);
            const RightHandSide &rhs = rightHandSides.at(rhsIndex);
            if (alreadyHandledThese.count(rhsIndex) == 1) {
                continue;
            }
            alreadyHandledThese.insert(rhsIndex);

            std::set<RightHandSideIndex> alreadyChainedWith;
            for (TransIndex t2 : midout) {
                assert (mid != getTransTarget(t2)); //selfloops cannot occur ("V" check above)
                if (Timeout::soft()) break;

                RightHandSideIndex rhsIndex2 = getTransData(t2);
                const RightHandSide &rhs2 = rightHandSides.at(rhsIndex2);

                if (alreadyChainedWith.count(rhsIndex2) == 1) {
                    continue;
                }
                alreadyChainedWith.insert(rhsIndex2);

                RightHandSide rhsCopy = rhs;
                if (chainRightHandSides(rhsCopy, mid, rhs2)) {
                    addRightHandSide(node, std::move(rhsCopy));
                    Stats::add(Stats::ContractBranch);

                } else {
                    //if UNSAT, add a new dummy node to keep the first part of the transition (which is removed below)
                    if (Expression(rhsCopy.cost.toGiNaC(true)).getComplexity() > 0) {
                        assert(mid != NULLNODE);

                        // replace function symbol "mid" by a new one
                        // by rewriting using the rule mid(x_1, ..., x_n) -> mid'(x_1, ..., x_n)
                        FunctionSymbolIndex oldIndex = (FunctionSymbolIndex)mid;
                        FunctionSymbolIndex newIndex = itrs.addFunctionSymbolVariant(oldIndex);
                        nodes.insert((NodeIndex)newIndex);
                        // Important: These calls happen after addFunctionSymbolVariant()
                        // (adding a new function symbol to the vector might move the old ones in memory)
                        const FunctionSymbol &oldFunSym = itrs.getFunctionSymbol(oldIndex);
                        const FunctionSymbol &newFunSym = itrs.getFunctionSymbol(newIndex);

                        // build the definition needed for replacing mid by mid'
                        std::vector<TT::Expression> args;
                        for (VariableIndex var : oldFunSym.getArguments()) {
                            args.push_back(TT::Expression(itrs.getGinacSymbol(var)));
                        }

                        // mid'(x_1, ..., x_n)
                        TT::Expression def(newIndex, newFunSym.getName(), args);
                        TT::Expression null(GiNaC::numeric(0));
                        std::vector<TT::Expression> nullVector;
                        TT::FunctionDefinition funDef(itrs, oldIndex, def, null, nullVector);

                        // evaluate the function
                        rhsCopy.term = rhsCopy.term.evaluateFunction(funDef, nullptr, nullptr);
                        rhsCopy.cost = rhsCopy.cost.evaluateFunction(funDef, nullptr, nullptr);
                        for (TT::Expression &ex : rhsCopy.guard) {
                            ex = ex.evaluateFunction(funDef, nullptr, nullptr);
                        }

                        addRightHandSide(node, std::move(rhsCopy));
                    }
                }
            }

            removeThese.insert(getTransData(t));
            changed = true;
            if (Timeout::soft()) break;
        }

        for (RightHandSideIndex rhsIndex : removeThese) {
            removeRightHandSide(node, rhsIndex);
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
        removeConstLeavesAndUnreachable();
    }

    return modified;
}


bool RecursionGraph::canNest(const RightHandSide &inner, FunctionSymbolIndex index, const RightHandSide &outer) const {
    set<string> innerguard;
    for (const TT::Expression &ex : inner.guard) Expression(ex.toGiNaC()).collectVariableNames(innerguard);
    for (const auto &it : outer.toLegacyTransition(itrs, outerUpdateVar, index).update) {
        if (innerguard.count(itrs.getVariableName(it.first)) > 0) {
            return true;
        }
    }

    return false;
}


bool RecursionGraph::chainSimpleLoops(NodeIndex node) {
    debugGraph("Chaining simple loops.");
    assert(node != initial);
    assert(node != NULLNODE);
    assert(!getTransFromTo(node, node).empty());

    bool changed = false;

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
    debugGraph("(" << itrs.getFunctionSymbolName(node) << ")");

    for (TransIndex simpleLoop : getTransFromTo(node, node)) {
        RightHandSideIndex simpleLoopRhsIndex = getTransData(simpleLoop);
        const RightHandSide &simpleLoopRhs = rightHandSides.at(simpleLoopRhsIndex);

        for (std::pair<TransIndex,bool> &pair : transitions) {
            RightHandSideIndex rhsIndex = getTransData(pair.first);

            RightHandSide rhsCopy = rightHandSides.at(rhsIndex);
            if (chainRightHandSides(rhsCopy, node, simpleLoopRhs)) {
                addRightHandSide(getTransSource(pair.first), rhsCopy);
                Stats::add(Stats::ContractLinear);
                pair.second = true;

                changed = true;
            }

        }

        debugGraph("removing rhs for simple loop " << simpleLoop);
        removeRightHandSide(node, simpleLoopRhsIndex);
    }

    for (const std::pair<TransIndex,bool> &pair : transitions) {
        if (pair.second && !(addTransitionToSkipLoops.count(node) > 0)) {
            debugGraph("removing transition (+ rhs)" << pair.first);
            RightHandSideIndex rhsIndex = getTransData(pair.first);
            removeRightHandSide(getTransSource(pair.first), rhsIndex);
        }
    }

    return changed;
}


bool RecursionGraph::accelerateSimpleLoops(NodeIndex node) {
    vector<TransIndex> loops;
    for (TransIndex trans : getTransFromTo(node, node)) {
        const RightHandSide &rhs = rightHandSides.at(getTransData(trans));

        if (rhs.isLegacyTransition(itrs)) {
            loops.push_back(trans);

        }
    }

    if (loops.empty()) {
        return false;
    }

    proofout << "Eliminating " << loops.size() << " self-loops for location ";
    itrs.printLhs(node, proofout);
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
    set<TransIndex> todo_remove;
    set<TransIndex> added_unranked;
    map<TransIndex,TransIndex> map_to_original; //maps ranked transition to the original transition

    //use index to iterate, as transitions are appended to loops while iterating
    int oldloopcount = loops.size();
    for (int lopidx=0; lopidx < loops.size(); ++lopidx) {
        if (Timeout::soft()) goto timeout;
        TransIndex tidx = loops[lopidx];
        RightHandSide &rhs = rightHandSides.at(getTransData(tidx));

        //remove the original selfloop later
        todo_remove.insert(tidx);

        //abort early on INF selfloops
        if (Expression(rhs.cost.toGiNaC()).isInfty()) {
            addRightHandSide(node, rhs);
            continue;
        }

#ifdef SELFLOOPS_ALWAYS_SIMPLIFY
        Timing::start(Timing::Preprocess);

        if (PreprocessITRS::simplifyRightHandSide(itrs, rhs)) {
            debugGraph("Simplified transition before Farkas");
        }
        Timing::done(Timing::Preprocess);
#endif

        Expression rankfunc;
        pair<VariableIndex,VariableIndex> conflictVar;
        Transition data = rhs.toLegacyTransition(itrs, outerUpdateVar, node); //note: data possibly modified by instantiation in farkas
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
            loops.push_back(addLegacyTransition(node, node, dataAB));

            //add B > A to the guard, process resulting selfloop later
            Transition dataBA = data; //copy
            dataBA.guard.push_back(itrs.getGinacSymbol(B) > itrs.getGinacSymbol(A));
            loops.push_back(addLegacyTransition(node, node, dataBA));

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
                TransIndex newIdx = addLegacyTransition(node,node,data);
                proofout << "  Self-Loop " << tidx << " has unbounded runtime, resulting in the new transition " << newIdx << "." << endl;
            }
            else if (result == FarkasMeterGenerator::Nonlinear) {
                Stats::add(Stats::SelfloopNoRank);
                debugGraph("Farkas nonlinear!");
                addTransitionToSkipLoops.insert(node);
                addRightHandSide(node, rhs); //keep old
            }
            else if (result == FarkasMeterGenerator::Unsat) {
                Stats::add(Stats::SelfloopNoRank);
                debugGraph("Farkas unsat!");
                addTransitionToSkipLoops.insert(node);
                added_unranked.insert(addLegacyTransition(node,node,data)); //keep old, mark as unsat
            }
            else if (result == FarkasMeterGenerator::Success) {
                debugGraph("RANK: " << rankfunc);
                if (!Recurrence::calcIterated(itrs,data,rankfunc)) {
                    //do not add to added_unranked, as this will probably not help with nested loops
                    Stats::add(Stats::SelfloopNoUpdate);
                    addTransitionToSkipLoops.insert(node);
                    addRightHandSide(node, rhs); //keep old

                } else {
                    Stats::add(Stats::SelfloopRanked);
                    TransIndex tnew = addLegacyTransition(node,node,data);
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
            const RightHandSide &innerRhs = rightHandSides.at(getTransData(inner));

            for (TransIndex outer : added_unranked) {
                if (Timeout::soft()) goto timeout;

                //dont nest a loop with itself or with its original transition (save time)
                if (inner == outer) continue;
                if (map_to_original[inner] == outer) continue;

                const RightHandSide &outerRhs = rightHandSides.at(getTransData(outer));

                //dont nest if the inner loop has constant runtime
                if (Expression(innerRhs.cost.toGiNaC()).getComplexity() == 0) continue;

                //check if we can nest at all
                if (!canNest(innerRhs, node, outerRhs)) continue;

                //inner loop first
                RightHandSide loop = innerRhs;
                if (chainRightHandSides(loop, node, outerRhs)) {
                    Complexity oldcpx = Expression(loop.cost.toGiNaC()).getComplexity();

                    Transition loopLegacy = loop.toLegacyTransition(itrs, outerUpdateVar, node);
                    if (try_rank(loopLegacy) && loopLegacy.cost.getComplexity() > oldcpx) {
                        proofout << "  and nested parallel self-loops " << outer << " (outer loop) and " << inner << " (inner loop), obtaining the new transitions: ";
                        changed = true;
                        todo_remove.insert(outer); //remove the previously unsat loop
                        TransIndex tnew = addLegacyTransition(node,node,loopLegacy);
                        added_nested.insert(tnew);
                        proofout << tnew;

                        //try one outer iteration first as well (this is costly, but nested loops are often quadratic!)
                        RightHandSide pre = outerRhs;
                        RightHandSide loopAsRhs = loopLegacy.toRightHandSide(itrs, node);
                        if (chainRightHandSides(pre, node, loopAsRhs)) {
                            TransIndex tnew = addLegacyRightHandSide(node, node, pre);
                            added_nested.insert(tnew);
                            proofout << ", " << tnew;
                        }
                        proofout << "." << endl;
                    }
                }

                //outer loop first
                loop = outerRhs;
                if (chainRightHandSides(loop, node, innerRhs)) {
                    Complexity oldcpx = Expression(loop.cost.toGiNaC()).getComplexity();

                    Transition loopLegacy = loop.toLegacyTransition(itrs, outerUpdateVar, node);
                    if (try_rank(loopLegacy) && loopLegacy.cost.getComplexity() > oldcpx) {
                        proofout << "  and nested parallel self-loops " << outer << " (outer loop) and " << inner << " (inner loop), obtaining the new transitions: ";
                        changed = true;
                        todo_remove.insert(outer); //remove the previously unsat loop
                        TransIndex tnew = addLegacyTransition(node,node,loopLegacy);
                        added_nested.insert(tnew);
                        proofout << tnew;

                        //try one inner iteration first as well (this is costly, but nested loops are often quadratic!)
                        RightHandSide pre = innerRhs;
                        RightHandSide loopAsRhs = loopLegacy.toRightHandSide(itrs, node);
                        if (chainRightHandSides(pre, node, loopAsRhs)) {
                            TransIndex tnew = addLegacyRightHandSide(node, node, pre);
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
                RightHandSide chainedRhs = rightHandSides.at(getTransData(first));
                const RightHandSide &secondRhs = rightHandSides.at(getTransData(second));
                if (chainRightHandSides(chainedRhs, secondRhs)) {
                    TransIndex newtrans = addLegacyRightHandSide(node, node, chainedRhs);
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

        rightHandSides.erase(getTransData(tidx));
        removeTrans(tidx);
    }
    proofout << "." << endl;

    removeDuplicateTransitions(getTransFromTo(node,node));

    return true; //always changed as old transition is removed
}


void RecursionGraph::removeIncorrectTransitionsToNullNode() {
    vector<TransIndex> toNull = getTransTo(NULLNODE);
    for (TransIndex trans : toNull) {
        if (!rightHandSides.at(getTransData(trans)).term.hasNoFunctionSymbols()) {
            removeTrans(trans);
        }
    }
}


bool RecursionGraph::compareRightHandSides(RightHandSideIndex indexA, RightHandSideIndex indexB) const {
    assert(indexA != indexB);

    const RightHandSide &a = rightHandSides.at(indexA);
    const RightHandSide &b = rightHandSides.at(indexB);
    if (a.guard.size() != b.guard.size()) return false;
    if (!a.cost.hasNoFunctionSymbols() || !b.cost.hasNoFunctionSymbols()) {
        return false;
    }

    if (!GiNaC::is_a<GiNaC::numeric>(a.cost.toGiNaC()-b.cost.toGiNaC())) return false; //cost equal up to constants
    if (a.term.isSimple() && b.term.isSimple()) {
        for (int i = 0; i < a.term.nops(); i++) {
            if (!a.term.op(i).toGiNaC().is_equal(b.term.op(i).toGiNaC())) {
                return false;
            }
        }

    } else {
        return false;
    }

    for (int i=0; i < a.guard.size(); ++i) {
        if (!a.guard[i].toGiNaC(true).is_equal(b.guard[i].toGiNaC(true))) {
            return false;
        }
    }
    return true;
}


bool RecursionGraph::removeConstLeavesAndUnreachable() {
    bool changed = false;
    set<NodeIndex> reached;
    function<void(NodeIndex)> dfs_remove;
    dfs_remove = [&](NodeIndex curr) {
        if (reached.insert(curr).second == false) return; //already present

        std::set<RightHandSideIndex> rhss;
        for (TransIndex trans : getTransFrom(curr)) {
            rhss.insert(getTransData(trans));
        }

        for (RightHandSideIndex rhsIndex : rhss) {
            const RightHandSide &rhs = rightHandSides.at(rhsIndex);

            bool allLeaves = true;
            std::set<NodeIndex> succs = std::move(getSuccessorsOfRhs(rhs));
            for (NodeIndex succ : succs) {
                dfs_remove(succ);

                if (succ == NULLNODE || !getTransFrom(succ).empty()) {
                    allLeaves = false;
                    // do not break so that all successors will be simplified
                }
            }

            if (allLeaves) {
                // toGiNaC(true) -> Substitute function symbols by variables
                Expression costGiNaC = rhs.cost.toGiNaC(true);
                if (costGiNaC.getComplexity() <= 0) {
                    removeRightHandSide(curr, rhsIndex);
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

bool RecursionGraph::removeIrrelevantTransitions(NodeIndex curr, set<NodeIndex> &visited) {
    if (visited.insert(curr).second == false) return true; //already seen, remove any transitions forminig a loop

    for (NodeIndex next : getSuccessors(curr)) {
        if (Timeout::hard()) return false;
        if (removeIrrelevantTransitions(next,visited)) {
            //only const costs below next, so keep only non-const transitions to next
            std::vector<TransIndex> checktrans = getTransFromTo(curr,next);
            for (TransIndex trans : checktrans) {
                RightHandSideIndex rhsIndex = getTransData(trans);
                const RightHandSide &rhs = rightHandSides.at(rhsIndex);

                if (Expression(rhs.cost.toGiNaC()).getComplexity() <= 0)  {
                    removeRightHandSide(curr, rhsIndex);
                }
            }
        }
    }
    return getTransFrom(curr).empty(); //if true, curr is not of any interest anymore
}


bool RecursionGraph::rightHandSideIsEmpty(NodeIndex node, const RightHandSide &rhs) const {
    if (rhs.guard.size() > 0) {
        return false;
    }

    if (!(rhs.cost.info(TT::InfoFlag::Number) && rhs.cost.toGiNaC().is_zero())) {
        return false;
    }

    if (!rhs.term.info(TT::InfoFlag::FunctionSymbol)) {
        return false;
    }

    const FunctionSymbol &funSymbol = itrs.getFunctionSymbol((FunctionSymbolIndex)node);
    const std::vector<VariableIndex> &vars = funSymbol.getArguments();
    assert(vars.size() == rhs.term.nops());
    for (int i = 0; i < vars.size(); ++i) {
        TT::Expression arg = std::move(rhs.term.op(i));

        if (!arg.info(TT::InfoFlag::Variable)) {
            return false;
        }

        ExprSymbol ginacVar = itrs.getGinacSymbol(vars[i]);
        if (!arg.toGiNaC().is_equal(ginacVar)) {
            return false;
        }
    }

    return true;
}