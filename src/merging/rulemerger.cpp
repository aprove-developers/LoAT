/*  This file is part of LoAT.
 *  Copyright (c) 2019 Florian Frohn
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

#include "rulemerger.hpp"
#include "../its/itsproblem.hpp"
#include "../z3/z3context.hpp"
#include "../z3/z3solver.hpp"

namespace merging {

    typedef RuleMerger Self;

    bool Self::mergeRules(ITSProblem &its) {
        return RuleMerger(its).apply();
    }

    Self::RuleMerger(ITSProblem &its): its(its) {}

    bool Self::apply() {
        bool changed = false;
        const std::vector<std::vector<TransIdx>> &candidates = findCandidates();
        for (const std::vector<TransIdx> &cs: candidates) {
            changed |= merge(cs);
            if (Timeout::soft()) return changed;
        }
        return changed;
    }

    std::vector<std::vector<TransIdx>> Self::findCandidates() {
        std::vector<std::vector<TransIdx>> res;
        for (const LocationIdx &loc: its.getLocations()) {
            std::set<TransIdx> out = its.getTransitionsFrom(loc);
            std::vector<std::vector<TransIdx>> subRes;
            for (const TransIdx &t: out) {
                bool found = false;
                for (std::vector<TransIdx> &eqClass: subRes) {
                    if (sameEquivalenceClass(t, *eqClass.begin())) {
                        eqClass.push_back(t);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    subRes.push_back({t});
                }
            }
            for (const std::vector<TransIdx > &eqClass: subRes) {
                if (eqClass.size() > 1) {
                    res.push_back(eqClass);
                }
            }
        }
        return res;
    }

    bool Self::sameEquivalenceClass(const TransIdx &t1, const TransIdx &t2) {
        const Rule &r1 = its.getRule(t1);
        const Rule &r2 = its.getRule(t2);
        if (r1.getLhsLoc() != r2.getLhsLoc()) {
            return false;
        }
        if (r1.getRhss().size() != r2.getRhss().size()) {
            return false;
        }
        // note that costs might be incomparable
        if (!(r1.getCost() >= r2.getCost()) && !(r1.getCost() <= r2.getCost())) {
            return false;
        }
        const std::vector<RuleRhs> &r2Rhss = r2.getRhss();
        for (const RuleRhs &rhs: r1.getRhss()) {
            if (std::find(r2Rhss.begin(), r2Rhss.end(), rhs) == r2Rhss.end()) {
                return false;
            }
        }
        return true;
    }

    bool Self::merge(const std::vector<TransIdx> &candidates) {
        std::vector<std::pair<Rule, TransIdx>> rules;
        std::set<std::pair<TransIdx, TransIdx>> done;
        bool changed = false;
        for (const TransIdx &t: candidates) {
            const Rule &r = its.getRule(t);
            rules.push_back({r, t});
        }
        auto it1 = rules.begin();
        while (it1 != rules.end() - 1) {
            bool restart = false;
            auto it2 = it1 + 1;
            while (it2 != rules.end()) {
                if (Timeout::soft()) return changed;
                if (done.find({it1->second, it2->second}) == done.end()) {
                    done.insert({it1->second, it2->second});
                    const option<Rule> &merged = merge(it1->first, it2->first);
                    if (merged) {
                        changed = true;
                        const TransIdx &t = its.addRule(merged.get());
                        its.removeRule(it1->second);
                        its.removeRule(it2->second);
                        rules.erase(it2);
                        rules.erase(it1);
                        rules.push_back({merged.get(), t});
                        restart = true;
                        break;
                    }
                }
                it2++;
            }
            if (restart) {
                it1 = rules.begin();
            } else {
                it1++;
            }
        }
        return changed;
    }

    option<Rule> Self::merge(const Rule &r1, const Rule &r2) {
        const GuardList &g1 = r1.getGuard();
        const GuardList &g2 = r2.getGuard();
        const Expression &c1 = r1.getCost();
        const Expression &c2 = r2.getCost();
        GuardList notInIntersection1(g1);
        GuardList intersection;
        for (const Expression &e: g2) {
            auto it = std::find(notInIntersection1.begin(), notInIntersection1.end(), e);
            if (it != notInIntersection1.end()) {
                intersection.push_back(*it);
                notInIntersection1.erase(it);
            }
        }
        GuardList notInIntersection2(g2);
        for (const Expression &e: g1) {
            auto it = std::find(notInIntersection2.begin(), notInIntersection2.end(), e);
            if (it != notInIntersection2.end()) {
                notInIntersection2.erase(it);
            }
        }
        Z3Context ctx;
        Z3Solver solver(ctx);
        z3::expr_vector v1(ctx);
        for (const Expression &e: notInIntersection1) {
            v1.push_back(!e.toZ3(ctx));
        }
        z3::expr_vector v2(ctx);
        for (const Expression &e: notInIntersection2) {
            v2.push_back(!e.toZ3(ctx));
        }
        solver.add(z3::mk_or(v1) && z3::mk_or(v2));
        if (solver.check() == z3::check_result::unsat) {
            const Expression &c = c1 >= c2 ? c1 : c2;
            Rule res(RuleLhs(r1.getLhsLoc(), intersection, c), r1.getRhss());
            return res;
        }
        return {};
    }

}
