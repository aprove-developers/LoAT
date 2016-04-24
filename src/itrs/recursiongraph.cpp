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

#include <purrs.hh>

using namespace std;
namespace Purrs = Parma_Recurrence_Relation_Solver;

const NodeIndex RecursionGraph::NULLNODE = -1;

std::ostream& operator<<(std::ostream &os, const RightHandSide &rhs) {
    os << *(rhs.term) << ", [";
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
    class VarSubVisitor : public TT::Term::Visitor {
    public:
        VarSubVisitor(const ITRSProblem &itrs) {
        }

        void visit(TT::GiNaCExpression &expr) override {
            GiNaC::exmap sub;
            for (const ExprSymbol &sym : Expression(expr.getExpression()).getVariables()) {
                sub.emplace(sym, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
            }

            expr.setExpression(expr.getExpression().subs(sub));
        }
    };

    assert(node != NULLNODE);
    debugRecGraph("solving recursion for " << itrs.getFunctionSymbol(node).getName());

    const FunctionSymbol& funSymbol = itrs.getFunctionSymbol((FunctionSymbolIndex)node);
    VariableIndex critVar = funSymbol.getArguments()[0];

    std::vector<TransIndex> recursiveCalls = getTransFromTo(node, node);
    RightHandSideIndex rhsIndex = getTransData(recursiveCalls[0]);
    const RightHandSide &rhs = rightHandSides.at(rhsIndex);
    debugRecGraph(rhs);

    VarSubVisitor vis(itrs);
    rhs.term->traverse(vis);

    Purrs::Expr recurrence = rhs.term->toPurrs();
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(recurrence);
        rec.set_initial_conditions({ {0, 0} }); //costs for no iterations are hopefully 0

        debugRecGraph("recurrence: " << recurrence);

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            return false;
        }

        rec.exact_solution(exact);
    } catch (...) {
        debugRecGraph("Purrs failed on x(n) = " << recurrence << " with initial x(0)=0");
        return false;
    }

    GiNaC::ex result = exact.toGiNaC();
    GiNaC::exmap sub;
    sub.emplace(Purrs::Expr(Purrs::Recurrence::n).toGiNaC(), itrs.getGinacSymbol(critVar));
    result = result.subs(sub);

    debugRecGraph("Purrs result:" << result);
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


void RecursionGraph::addRule(const ITRSRule &rule) {
    RightHandSide rhs;
    rhs.guard = rule.guard;
    rhs.term = rule.rhs;
    rhs.cost = rule.cost;

    NodeIndex src = (NodeIndex)rule.lhs;
    std::vector<NodeIndex> dsts = (std::vector<NodeIndex>)rhs.term->getFunctionSymbolsAsVector();
    if (dsts.empty()) {
        dsts.push_back(NULLNODE);
    }

    RightHandSideIndex rhsIndex = nextRightHandSide++;
    rightHandSides.insert(std::make_pair(rhsIndex, rhs));

    for (NodeIndex dst : dsts) {
        addTrans(src, dst, rhsIndex);
    }
}