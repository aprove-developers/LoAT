#ifndef ITRS_TERM_H
#define ITRS_TERM_H

#include <memory>
#include <ostream>
#include <set>
#include <vector>

#include <ginac/ginac.h>
#include <purrs.hh>

#include "exceptions.h"
#include "expression.h"

namespace Purrs = Parma_Recurrence_Relation_Solver;

namespace TT {
class Term;
}

// typedefs for readability
typedef int FunctionSymbolIndex;
typedef int VariableIndex;
typedef std::map<ExprSymbol,std::shared_ptr<TT::Term>> Substitution;
typedef std::vector<std::shared_ptr<TT::Term>> TermVector;

class FunctionSymbol;
class ITRSProblem;

namespace TT {

    class Relation;
    class Addition;
    class Subtraction;
    class Multiplication;
    class FunctionSymbol;
    class GiNaCExpression;

// helper class for evaluateFunction()
class FunctionDefinition {
public:
    FunctionDefinition(const ::FunctionSymbol &fs,
                       const std::shared_ptr<Term> &def,
                       const std::shared_ptr<Term> &cost,
                       const TermVector &guard);
    const ::FunctionSymbol& getFunctionSymbol() const;
    std::shared_ptr<Term> getDefinition() const;
    std::shared_ptr<Term> getCost() const;
    const TermVector& getGuard() const;

private:
    const ::FunctionSymbol &functionSymbol;
    const std::shared_ptr<Term> definition;
    const std::shared_ptr<Term> cost;
    const TermVector &guard;
};

/**
 * Represents a term (lhs/rhs) of a rule as an AST.
 */
class Term {
public:
    class Visitor {
    public:
        virtual void visitPre(Relation &rel) {};
        virtual void visitIn(Relation &rel) {};
        virtual void visitPost(Relation &rel) {};

        virtual void visitPre(Addition &add) {};
        virtual void visitIn(Addition &add) {};
        virtual void visitPost(Addition &add) {};

        virtual void visitPre(Subtraction &sub) {};
        virtual void visitIn(Subtraction &sub) {};
        virtual void visitPost(Subtraction &sub) {};

        virtual void visitPre(Multiplication &mul) {};
        virtual void visitIn(Multiplication &mul) {};
        virtual void visitPost(Multiplication &mul) {};

        virtual void visitPre(FunctionSymbol &funcSym) {};
        virtual void visitIn(FunctionSymbol &funcSym) {};
        virtual void visitPost(FunctionSymbol &funcSym) {};

        virtual void visit(GiNaCExpression &expr) {};
    };

    class ConstVisitor {
    public:
        virtual void visitPre(const Relation &rel) {};
        virtual void visitIn(const Relation &rel) {};
        virtual void visitPost(const Relation &rel) {};

        virtual void visitPre(const Addition &add) {};
        virtual void visitIn(const Addition &add) {};
        virtual void visitPost(const Addition &add) {};

        virtual void visitPre(const Subtraction &sub) {};
        virtual void visitIn(const Subtraction &sub) {};
        virtual void visitPost(const Subtraction &sub) {};

        virtual void visitPre(const Multiplication &mul) {};
        virtual void visitIn(const Multiplication &mul) {};
        virtual void visitPost(const Multiplication &mul) {};

        virtual void visitPre(const FunctionSymbol &funcSym) {};
        virtual void visitIn(const FunctionSymbol &funcSym) {};
        virtual void visitPost(const FunctionSymbol &funcSym) {};

        virtual void visit(const GiNaCExpression &expr) {};
    };

public:
    EXCEPTION(UnsupportedExpressionException, CustomException);
    static std::shared_ptr<Term> fromGiNaC(const ITRSProblem &itrs, const GiNaC::ex &ex);

public:
    Term(const ITRSProblem &itrs);
    virtual ~Term();

    const ITRSProblem& getITRSProblem() const;

    void collectVariables(ExprSymbolSet &set) const;
    ExprSymbolSet getVariables() const;

    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;
    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;

    bool containsNoFunctionSymbols() const;

    virtual void traverse(Visitor &visitor) = 0;
    virtual void traverse(ConstVisitor &visitor) const = 0;

    virtual std::shared_ptr<Term> copy() const = 0;
    virtual std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   std::shared_ptr<Term> &addToCost, TermVector &addToGuard) = 0;
    virtual std::shared_ptr<Term> substitute(const Substitution &sub) const = 0;

    EXCEPTION(UnsupportedOperationException, CustomException);
    virtual GiNaC::ex toGiNaC() const;
    virtual Purrs::Expr toPurrs() const;
    virtual std::shared_ptr<Term> ginacify() const = 0;
    virtual std::shared_ptr<Term> unGinacify() const = 0;

private:
    const ITRSProblem &itrs;
};

std::ostream& operator<<(std::ostream &os, const Term &term);


class Relation : public Term {
public:
    enum Type { EQUAL = 0, GREATER, GREATER_EQUAL };
    static const char* TypeNames[];

public:
    Relation(const ITRSProblem &itrs, Type type, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(Visitor &visitor) override;
    void traverse(ConstVisitor &visitor) const;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   std::shared_ptr<Term> &addToCost,
                                                   TermVector &addToGuard) override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;

    Type getType() const;

    GiNaC::ex toGiNaC() const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    Type type;
    std::shared_ptr<Term> l, r;
};

class Addition : public Term {
public:
    Addition(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(Visitor &visitor) override;
    void traverse(ConstVisitor &visitor) const;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   std::shared_ptr<Term> &addToCost,
                                                   TermVector &addToGuard) override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;

    GiNaC::ex toGiNaC() const override;
    Purrs::Expr toPurrs() const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    std::shared_ptr<Term> l, r;
};


class Subtraction : public Term {
public:
    Subtraction(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(Visitor &visitor) override;
    void traverse(ConstVisitor &visitor) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   std::shared_ptr<Term> &addToCost,
                                                   TermVector &addToGuard) override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;

    GiNaC::ex toGiNaC() const override;
    Purrs::Expr toPurrs() const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    std::shared_ptr<Term> l, r;
};


class Multiplication : public Term {
public:
    Multiplication(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(Visitor &visitor) override;
    void traverse(ConstVisitor &visitor) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   std::shared_ptr<Term> &addToCost,
                                                   TermVector &addToGuard) override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;

    GiNaC::ex toGiNaC() const override;
    Purrs::Expr toPurrs() const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    std::shared_ptr<Term> l, r;
};


class FunctionSymbol : public Term {
public:
    FunctionSymbol(const ITRSProblem &itrs, FunctionSymbolIndex functionSymbol, const std::vector<std::shared_ptr<Term>> &args);
    void traverse(Visitor &visitor) override;
    void traverse(ConstVisitor &visitor) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   std::shared_ptr<Term> &addToCost,
                                                   TermVector &addToGuard) override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;

    FunctionSymbolIndex getFunctionSymbol() const;

    Purrs::Expr toPurrs() const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    FunctionSymbolIndex functionSymbol;
    std::vector<std::shared_ptr<Term>> args;
};


class GiNaCExpression : public Term {
public:
    GiNaCExpression(const ITRSProblem &itrs, const GiNaC::ex &expr);
    void traverse(Visitor &visitor) override;
    void traverse(ConstVisitor &visitor) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   std::shared_ptr<Term> &addToCost,
                                                   TermVector &addToGuard) override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;

    GiNaC::ex getExpression() const;
    void setExpression(const GiNaC::ex &expr);

    GiNaC::ex toGiNaC() const override;
    Purrs::Expr toPurrs() const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    GiNaC::ex expression;
};

};

#endif // ITRS_TERM_H