//
// Created by ffrohn on 2/21/19.
//

#include "templates.h"

typedef Templates Self;

void Self::add(const Self::Template &t) {
    templates.push_back(t.t);
    vars_.insert(t.vars.begin(), t.vars.end());
    params_.insert(t.params.begin(), t.params.end());
}

const ExprSymbolSet& Self::params() const {
    return params_;
}

const ExprSymbolSet& Self::vars() const {
    return vars_;
}

bool Self::isParametric(const Expression &e) const {
    ExprSymbolSet eVars = e.getVariables();
    for (const ExprSymbol &x: params()) {
        if (eVars.count(x) > 0) {
            return true;
        }
    }
    return false;
}

const std::vector<Expression> Self::subs(const GiNaC::exmap &sigma) const {
    std::vector<Expression> res;
    for (Expression e: templates) {
        e.applySubs(sigma);
        res.push_back(e);
    }
    return res;
}

Self::iterator Self::begin() const {
    return templates.begin();
}

Self::iterator Self::end() const {
    return templates.end();
}
