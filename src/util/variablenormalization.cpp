//
// Created by ffrohn on 2/28/19.
//

#include "variablenormalization.h"

typedef VariableNormalization Self;

VariableNormalization::VariableNormalization(const VariableManager &varMan): varMan(varMan) { }

void Self::buildRenaming(const GiNaC::ex &e, const GiNaC::exset &varsToNormalize) {
    if (GiNaC::is_a<GiNaC::symbol>(e)) {
        const auto &symbol = GiNaC::ex_to<GiNaC::symbol>(e);
        if (varsToNormalize.count(symbol) > 0 && renaming.count(e) == 0) {
            if (counter >= vars.size()) {
                vars.push_back(varMan.getFreshUntrackedSymbol("x" + std::to_string(counter)));
            }
            renaming[e] = vars[counter];
            counter++;
        }
    }
    for (const GiNaC::ex &op: e) {
        buildRenaming(op, varsToNormalize);
    }
}

const GiNaC::ex Self::normalize(const GiNaC::ex &e, const GiNaC::exset &varsToNormalize) {
    reset();
    buildRenaming(e, varsToNormalize);
    GiNaC::ex res = e.subs(renaming);
    return res;
}

const std::vector<GiNaC::ex> Self::normalize(const std::vector<GiNaC::ex> &es, const GiNaC::exset &varsToNormalize) {
    reset();
    std::vector<GiNaC::ex> res;
    for (const GiNaC::ex &e: es) {
        res.push_back(normalize(e, varsToNormalize));
    }
    return res;
}

void Self::reset() {
    renaming.clear();
    counter = 0;
}