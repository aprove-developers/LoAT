//
// Created by ffrohn on 2/28/19.
//

#ifndef LOAT_UTIL_VARIABLE_NORMALIZATION_H
#define LOAT_UTIL_VARIABLE_NORMALIZATION_H


#include <expr/expression.h>
#include <its/variablemanager.h>

class VariableNormalization {

public:

    explicit VariableNormalization(const VariableManager &);

    const GiNaC::ex normalize(const GiNaC::ex &, const GiNaC::exset &);

    const std::vector<GiNaC::ex> normalize(const std::vector<GiNaC::ex> &, const GiNaC::exset &);

private:

    void reset();

    void buildRenaming(const GiNaC::ex &, const GiNaC::exset &);

    const VariableManager &varMan;
    int counter;
    GiNaC::exmap renaming;
    std::vector<GiNaC::symbol> vars;

};


#endif //LOAT_UTIL_VARIABLE_NORMALIZATION_H
