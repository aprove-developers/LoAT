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

#include "term.h"
#include "recursion.h"
#include "preprocessitrs.h"
#include "stats.h"
#include "timeout.h"
#include "z3toolbox.h"

using namespace std;
namespace Purrs = Parma_Recurrence_Relation_Solver;

const NodeIndex RecursionGraph::NULLNODE = -1;

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
    : itrs(itrs), nextRightHandSide(0) {
    nodes.insert(NULLNODE);

    for (FunctionSymbolIndex i = 0; i < itrs.getFunctionSymbolCount(); ++i) {
        nodes.insert((NodeIndex)i);
    }

    initial = itrs.getStartFunctionSymbol();

    for (const ITRSRule &rule : itrs.getRules()) {
        addRule(rule);
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

    RightHandSide defRhs;
    Recursion recursion(itrs, funSymbolIndex, rhss, defRhs.term, defRhs.cost, defRhs.guard);
    if (!recursion.solve()) {
        return false;
    }

    for (TransIndex index : transitions) {
        if (rhss.count(&rightHandSides.at(getTransData(index))) == 0) {
            debugRecGraph("transition " << index << " was used for solving the recursion, removing");
            removeTrans(index);
        }
    }

    debugRecGraph("adding a new rhs for the solved recursion");
    assert(defRhs.term.getFunctionSymbols().empty());
    RightHandSideIndex rhsIndex = nextRightHandSide++;
    rightHandSides.emplace(rhsIndex, defRhs);
    addTrans(node, NULLNODE, rhsIndex);

    return true;
}


void RecursionGraph::print(ostream &s) const {
    auto printVar = [&](VariableIndex vi) {
        s << vi;
        s << "[" << itrs.getVarname(vi) << "]";
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
    for (TransIndex idx : getAllTrans()) {
        if (Timeout::preprocessing()) return changed;
        RightHandSide &rhs = rightHandSides.at(getTransData(idx));
        changed = PreprocessITRS::simplifyRightHandSide(itrs, rhs) || changed;
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
    Stats::addStep("FlowGraph::chainLinear");

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


void RecursionGraph::addRule(const ITRSRule &rule) {
    RightHandSide rhs;
    for (const Expression &ex : rule.guard) {
        rhs.guard.push_back(TT::Expression(itrs, ex));
    }
    rhs.term = rule.rhs;
    rhs.cost = TT::Expression(itrs, rule.cost);

    NodeIndex src = (NodeIndex)rule.lhs;
    std::set<NodeIndex> dsts = (std::set<NodeIndex>)rhs.term.getFunctionSymbols();
    if (dsts.empty()) {
        dsts.insert(NULLNODE);
    }

    RightHandSideIndex rhsIndex = nextRightHandSide++;
    rightHandSides.emplace(rhsIndex, rhs);

    for (NodeIndex dst : dsts) {
        addTrans(src, dst, rhsIndex);
    }
}


void RecursionGraph::removeRightHandSide(NodeIndex node, RightHandSideIndex rhs) {
    for (TransIndex trans : getTransFrom(node)) {
        if (getTransData(trans) == rhs) {
            removeTrans(trans);
        }
    }
    rightHandSides.erase(rhs);
}


bool RecursionGraph::chainRightHandSides(RightHandSide &rhs,
                                         const FunctionSymbolIndex funSymbolIndex,
                                         const RightHandSide &followRhs) const {
    const FunctionSymbol &funSymbol = itrs.getFunctionSymbol(funSymbolIndex);
    const TT::FunctionDefinition funDef(funSymbolIndex, followRhs.term, followRhs.cost, followRhs.guard);

    // perform rewriting on a copy of rhs
    RightHandSide rhsCopy(rhs);
    rhsCopy.term = rhsCopy.term.evaluateFunction(funDef, &rhsCopy.cost, &rhsCopy.guard).ginacify();
    rhsCopy.cost = rhsCopy.cost.evaluateFunction(funDef, nullptr, &rhsCopy.guard).ginacify();
    for (int i = 0; i < rhsCopy.guard.size(); ++i) {
           // the following call might add new elements to rhsCopy.guard
           rhsCopy.guard[i] = rhsCopy.guard[i].evaluateFunction(funDef, nullptr, &rhsCopy.guard).ginacify();
    }


    GuardList funSymbolFreeGuard;
    for (const TT::Expression &ex : rhsCopy.guard) {
        if (!ex.containsNoFunctionSymbols()) {
            // toGiNaC(true) -> Substitute function symbols by fresh variables
            funSymbolFreeGuard.push_back(ex.toGiNaC(true));
        }
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
    if ((rhs.cost.containsNoFunctionSymbols() && Expression(rhs.cost.toGiNaC()).isInfty())
        || (followRhs.cost.containsNoFunctionSymbols() && Expression(followRhs.cost.toGiNaC()).isInfty())) {
        rhs.cost = TT::Expression(itrs, Expression::Infty);

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


void RecursionGraph::removeIncorrectTransitionsToNullNode() {
    vector<TransIndex> toNull = getTransTo(NULLNODE);
    for (TransIndex trans : toNull) {
        if (!rightHandSides.at(getTransData(trans)).term.containsNoFunctionSymbols()) {
            removeTrans(trans);
        }
    }
}


bool RecursionGraph::compareRightHandSides(RightHandSideIndex indexA, RightHandSideIndex indexB) const {
    assert(indexA != indexB);

    const RightHandSide &a = rightHandSides.at(indexA);
    const RightHandSide &b = rightHandSides.at(indexB);
    if (a.guard.size() != b.guard.size()) return false;
    if (!a.cost.containsNoFunctionSymbols() || !b.cost.containsNoFunctionSymbols()) {
        return false;
    }

    if (!GiNaC::is_a<GiNaC::numeric>(a.cost.toGiNaC()-b.cost.toGiNaC())) return false; //cost equal up to constants
    if (a.term.info(TT::InfoFlag::FunctionSymbol)
        && b.term.info(TT::InfoFlag::FunctionSymbol)
        && a.term.containsExactlyOneFunctionSymbol()
        && b.term.containsExactlyOneFunctionSymbol()) {
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
        for (NodeIndex next : getSuccessors(curr)) {
            //recurse
            dfs_remove(next);

            //if next is (now) a leaf, remove const transitions to next
            if (!getTransFrom(next).empty()) continue;
            for (TransIndex trans : getTransFromTo(curr,next)) {
                RightHandSideIndex rhsIndex = getTransData(trans);
                const RightHandSide &rhs = rightHandSides.at(rhsIndex);

                // toGiNaC(true) -> Substitute function symbols by variables
                Expression costGiNaC = rhs.cost.toGiNaC(true);
                if (rhs.term.containsExactlyOneFunctionSymbol()
                    && costGiNaC.getComplexity() <= 0) {
                    removeTrans(trans);
                    rightHandSides.erase(rhsIndex);
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
