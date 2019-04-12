//
// Created by ffrohn on 2/21/19.
//

#ifndef LOAT_STRENGTHENING_TEMPLATES_H
#define LOAT_STRENGTHENING_TEMPLATES_H


#include <boost/detail/iterator.hpp>
#include "../expr/expression.hpp"
#include <boost/concept_check.hpp>

class Templates {

public:

    struct Template {

        Template(Expression t, ExprSymbolSet vars, ExprSymbolSet params) :
                t(std::move(t)), vars(std::move(vars)), params(std::move(params)) {}

        const Expression t;
        const ExprSymbolSet vars;
        const ExprSymbolSet params;

    };

    typedef std::vector<Expression>::const_iterator iterator;

    void add(const Template &t);

    const ExprSymbolSet& params() const;

    const ExprSymbolSet& vars() const;

    bool isParametric(const Expression &e) const;

    const std::vector<Expression> subs(const GiNaC::exmap &sigma) const;

    iterator begin() const;

    iterator end() const;

private:

    std::vector<Expression> templates;
    ExprSymbolSet params_;
    ExprSymbolSet vars_;

};


#endif //LOAT_STRENGTHENING_TEMPLATES_H
