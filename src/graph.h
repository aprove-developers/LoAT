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

#ifndef GRAPH_H
#define GRAPH_H


#include <vector>
#include <map>
#include <set>
#include <algorithm>

#include "debug.h"


//typedefs for readability
typedef int NodeIndex;
typedef int TransIndex;


/**
 * A simple directed graph, with data associated to the transitions
 */
template <typename T>
class Graph {
public:
    Graph() : nextIdx(0) {}

    /**
     * Add a new transition with the given associated data
     * @return the index of the added transition
     */
    TransIndex addTrans(NodeIndex from, NodeIndex to, T data) {
        assert(check() == Valid);
        TransIndex currIdx = nextIdx++;
        InternalTransition trans = {data,from,to};

        if (outgoing[from].count(to) > 0) debugGraph("Graph: [add] multiple edge from " << from << " -> " << to << "[" << data << "]");

        transitions[currIdx] = std::move(trans);
        predecessor[to].insert(from);
        outgoing[from][to].push_back(currIdx);
        assert(check() == Valid);
        return currIdx;
    }

    size_t getTransCount() const {
        return transitions.size();
    }

    std::vector<TransIndex> getTransFrom(NodeIndex from) const {
        std::vector<TransIndex> res;
        auto it = outgoing.find(from);
        if (it != outgoing.end()) {
            for (auto targetIt : it->second) {
                for (TransIndex trans : targetIt.second) {
                    res.push_back(trans);
                }
            }
        }
        return res;
    }

    std::vector<TransIndex> getTransFromTo(NodeIndex from, NodeIndex to) const {
        auto it = outgoing.find(from);
        if (it == outgoing.end()) return {};

        auto targetIt = it->second.find(to);
        if (targetIt == it->second.end()) return {};

        return targetIt->second;
    }

    std::set<NodeIndex> getSuccessors(NodeIndex node) const {
        std::set<NodeIndex> res;
        auto it = outgoing.find(node);
        if (it != outgoing.end()) {
            for (auto targetIt : it->second) {
                res.insert(targetIt.first);
            }
        }
        return res;
    }

    std::set<NodeIndex> getPredecessors(NodeIndex node) const {
        auto it = predecessor.find(node);
        return (it == predecessor.end()) ? std::set<NodeIndex>() : it->second;
    }

    inline T& getTransData(TransIndex idx) { return transitions[idx].data; }
    inline const T& getTransData(TransIndex idx) const { return transitions.at(idx).data; }
    inline NodeIndex getTransTarget(TransIndex idx) const { return transitions.at(idx).to; }

    std::vector<TransIndex> getAllTrans() const {
        std::vector<TransIndex> res;
        for (auto it : transitions) {
            res.push_back(it.first);
        }
        return res;
    }

    /**
     * Changes the given transition to point to the given new target
     * (no new transition is added, data is kept)
     */
    void changeTransTarget(TransIndex trans, NodeIndex newTarget) {
        assert(check() == Valid);
        removeTransFromGraph(trans);
        InternalTransition &t = transitions[trans];

        if (outgoing[t.from].count(newTarget) > 0) debugGraph("Graph: [change] multiple edge from " << t.from << " -> " << t.to << "[" << t.data << "]");

        outgoing[t.from][newTarget].push_back(trans);
        predecessor[newTarget].insert(t.from);

        t.to = newTarget;
        assert(check() == Valid);
    }

    /**
     * Splits the given node into two nodes, one for all incoming transitions (node) and one for all outgoing transitions (newOutgoing)
     * @note newOutgoing *must* be a fresh node index
     * @note afterwards, node has only incoming, newOutgoing only outgoing transitions and these two nodes are *not* connected to each other
     */
    void splitNode(NodeIndex node, NodeIndex newOutgoing) {
        assert(check() == Valid);
        assert(outgoing.count(newOutgoing) == 0 && predecessor.count(newOutgoing) == 0);

        //move outgoing to new node
        auto it = outgoing.find(node);
        if (it != outgoing.end()) {
            outgoing[newOutgoing] = it->second;
            outgoing.erase(it);
        }

        //adjust predecessor references for all successors
        for (NodeIndex succ : getSuccessors(newOutgoing)) {
            predecessor[succ].erase(node);
            predecessor[succ].insert(newOutgoing);
        }

        //adjust transitions from new node
        for (TransIndex idx : getTransFrom(newOutgoing)) {
            transitions[idx].from = newOutgoing;
        }
        assert(check() == Valid);
    }

    void removeNode(NodeIndex idx) {
        assert(check() == Valid);
        std::set<TransIndex> toRemove;
        //find outgoing transitions
        for (TransIndex out : getTransFrom(idx)) {
            toRemove.insert(out);
        }
        //find incoming transitions
        for (NodeIndex pre : getPredecessors(idx)) {
            for (TransIndex removeIdx : getTransFromTo(pre,idx)) {
                toRemove.insert(removeIdx);
            }
        }
        //remove it all
        for (TransIndex idx : toRemove) removeTrans(idx);
        assert(outgoing.count(idx) == 0);
        assert(predecessor.count(idx) == 0);
        assert(check() == Valid);
    }

    void removeTrans(TransIndex idx) {
        assert(check() == Valid);
        removeTransFromGraph(idx);
        transitions.erase(idx);
        assert(check() == Valid);
    }

    enum CheckResult { Valid=0, InvalidNode, EmptyMapEntry, UnknownTrans, InvalidTrans, UnusedTrans, DuplicateTrans, InvalidPred, InvalidPredCount };
    int check(std::set<NodeIndex> *nodes = nullptr) const {
#ifdef DEBUG_GRAPH
        int res = check_internal(nodes);
        if (res != 0) debugGraph("Graph ERROR: " << res);
        return res;
#else
        return Valid;
#endif
    }

private:
    //updates outgoing, predecessor to remove given trans from it current location
    void removeTransFromGraph(TransIndex trans) {
        InternalTransition &t = transitions[trans];

        std::vector<TransIndex> &vec = outgoing[t.from][t.to];
        vec.erase(std::find(vec.begin(),vec.end(),trans));
        if (vec.empty()) {
            outgoing[t.from].erase(t.to);
            if (outgoing[t.from].empty()) outgoing.erase(t.from);
            predecessor[t.to].erase(t.from);
            if (predecessor[t.to].empty()) predecessor.erase(t.to);
        }
    }

    /**
     * Function to check the integrity of all datastructures (used for debugging and testing only)
     */
    int check_internal(std::set<NodeIndex> *nodes) const {
        int edgecount = 0; //counting multi-edges once only
        std::set<TransIndex> seen;
        for (auto it : outgoing) {
            if (nodes && nodes->count(it.first) == 0) return InvalidNode;
            if (it.second.empty()) return EmptyMapEntry;
            for (auto it2 : it.second) {
                for (TransIndex trans : it2.second) {
                    if (nodes && nodes->count(it2.first) == 0) return InvalidNode;
                    if (transitions.count(trans) == 0) return UnknownTrans;
                    InternalTransition t = transitions.at(trans);
                    if (t.from != it.first || t.to != it2.first) return InvalidTrans;
                    if (seen.count(trans) > 0) return DuplicateTrans;
                    seen.insert(trans);
                }
                if (!it2.second.empty()) edgecount++;
            }
        }
        if (seen.size() != transitions.size()) return UnusedTrans;

        int cnt = 0;
        for (auto it : predecessor) {
            if (nodes && nodes->count(it.first) == 0) return InvalidNode;
            if (it.second.empty()) return EmptyMapEntry;
            for (NodeIndex node : it.second) {
                if (nodes && nodes->count(it.first) == 0) return InvalidNode;
                cnt++;
                //at() will throw an exception if outgoing trans does exist
                try {
                    for (TransIndex idx : outgoing.at(node).at(it.first)) {
                        if (idx < 0) return InvalidPred;
                    }
                } catch (...) { return InvalidPred; }
            }
        }
        if (cnt != edgecount) return InvalidPredCount;
        return Valid;
    }

private:
    struct InternalTransition {
        T data;
        NodeIndex from,to;
    };

    std::map<TransIndex,InternalTransition> transitions;
    std::map<NodeIndex,std::map<NodeIndex,std::vector<TransIndex>>> outgoing;
    std::map<NodeIndex,std::set<NodeIndex>> predecessor;

    TransIndex nextIdx;
};


#endif // GRAPH_H
