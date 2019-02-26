//
// Created by ffrohn on 2/21/19.
//

#include "modes.h"
#include "z3/z3context.h"

namespace strengthening {

    typedef Modes Self;

    const MaxSmtConstraints Self::invariance(const SmtConstraints &constraints, Z3Context &z3Ctx) {
        MaxSmtConstraints res;
        res.hard.insert(res.hard.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
        res.hard.insert(res.hard.end(), constraints.initiation.satisfiable.begin(), constraints.initiation.satisfiable.end());
        z3::expr_vector someConclusionInvariant(z3Ctx);
        for (const z3::expr &e: constraints.conclusionsInvariant) {
            res.soft.push_back(e);
            someConclusionInvariant.push_back(e);
        }
        res.hard.push_back(z3::mk_or(someConclusionInvariant));
        res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
        return res;
    }

    const MaxSmtConstraints Self::pseudoInvariance(const SmtConstraints &constraints, Z3Context &z3Ctx) {
        MaxSmtConstraints res;
        z3::expr_vector satisfiableForSomePredecessor(z3Ctx);
        for (const z3::expr &e: constraints.initiation.satisfiable) {
            satisfiableForSomePredecessor.push_back(e);
        }
        res.hard.push_back(z3::mk_or(satisfiableForSomePredecessor));
        res.soft.insert(res.soft.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
        z3::expr_vector someConclusionInvariant(z3Ctx);
        for (const z3::expr &e: constraints.conclusionsInvariant) {
            res.soft.push_back(e);
            someConclusionInvariant.push_back(e);
        }
        res.hard.push_back(z3::mk_or(someConclusionInvariant));
        res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
        return res;
    }

    const MaxSmtConstraints Self::monotonicity(const SmtConstraints &constraints, Z3Context &z3Ctx) {
        MaxSmtConstraints res;
        res.hard.insert(res.hard.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
        z3::expr_vector someConclusionMonotonic(z3Ctx);
        for (const z3::expr &e: constraints.conclusionsMonotonic) {
            res.soft.push_back(e);
            someConclusionMonotonic.push_back(e);
        }
        res.hard.push_back(z3::mk_or(someConclusionMonotonic));
        res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
        return res;
    }

    const MaxSmtConstraints Self::pseudoMonotonicity(const SmtConstraints &constraints, Z3Context &z3Ctx) {
        MaxSmtConstraints res;
        res.hard.insert(
                res.hard.end(),
                constraints.initiation.satisfiable.begin(),
                constraints.initiation.satisfiable.end());
        res.soft.insert(
                res.soft.end(),
                constraints.initiation.valid.begin(),
                constraints.initiation.valid.end());
        z3::expr_vector someConclusionMonotonic(z3Ctx);
        for (const z3::expr &e: constraints.conclusionsMonotonic) {
            res.soft.push_back(e);
            someConclusionMonotonic.push_back(e);
        }
        res.hard.push_back(z3::mk_or(someConclusionMonotonic));
        res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
        return res;
    }

}