//
// Created by ffrohn on 2/21/19.
//

#include "strengtheningmode.h"
#include "z3/z3context.h"

typedef StrengtheningTypes Types;
typedef StrengtheningMode Self;

const Types::MaxSmtConstraints Self::invariance(const Types::SmtConstraints &constraints, Z3Context &context) {
    Types::MaxSmtConstraints res;
    res.hard.insert(res.hard.end(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
    for (const z3::expr &e: constraints.initiation.satisfiable) {
        res.hard.push_back(e);
    }
    z3::expr_vector someConclusionInvariant(context);
    for (const z3::expr &e: constraints.conclusionsInvariant) {
        res.soft.push_back(e);
        someConclusionInvariant.push_back(e);
    }
    res.hard.push_back(z3::mk_or(someConclusionInvariant));
    res.hard.insert(res.hard.end(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
    return res;
}

const Types::MaxSmtConstraints Self::pseudoInvariance(const Types::SmtConstraints &constraints, Z3Context &context) {
    Types::MaxSmtConstraints res;
    z3::expr_vector satisfiableForSomePredecessor(context);
    for (const z3::expr &e: constraints.initiation.satisfiable) {
        satisfiableForSomePredecessor.push_back(e);
    }
    res.hard.push_back(z3::mk_or(satisfiableForSomePredecessor));
    res.soft.insert(res.soft.begin(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
    z3::expr_vector someConclusionInvariant(context);
    for (const z3::expr &e: constraints.conclusionsInvariant) {
        res.soft.push_back(e);
        someConclusionInvariant.push_back(e);
    }
    res.hard.push_back(z3::mk_or(someConclusionInvariant));
    res.hard.insert(res.hard.begin(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
    return res;
}

const Types::MaxSmtConstraints Self::monotonicity(const Types::SmtConstraints &constraints, Z3Context &context) {
    Types::MaxSmtConstraints res;
    res.hard.insert(res.hard.begin(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
    z3::expr_vector someConclusionMonotonic(context);
    for (const z3::expr &e: constraints.conclusionsMonotonic) {
        res.soft.push_back(e);
        someConclusionMonotonic.push_back(e);
    }
    res.hard.push_back(z3::mk_or(someConclusionMonotonic));
    res.hard.insert(res.hard.begin(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
    return res;
}

const Types::MaxSmtConstraints Self::pseudoMonotonicity(const Types::SmtConstraints &constraints, Z3Context &context) {
    Types::MaxSmtConstraints res;
    res.hard.insert(res.hard.begin(), constraints.initiation.satisfiable.begin(), constraints.initiation.satisfiable.end());
    res.soft.insert(res.soft.begin(), constraints.initiation.valid.begin(), constraints.initiation.valid.end());
    z3::expr_vector someConclusionMonotonic(context);
    for (const z3::expr &e: constraints.conclusionsMonotonic) {
        res.soft.push_back(e);
        someConclusionMonotonic.push_back(e);
    }
    res.hard.push_back(z3::mk_or(someConclusionMonotonic));
    res.hard.insert(res.hard.begin(), constraints.templatesInvariant.begin(), constraints.templatesInvariant.end());
    return res;
}
