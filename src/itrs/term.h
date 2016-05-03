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
class Expression;
}

// typedefs for readability
typedef int FunctionSymbolIndex;
typedef int VariableIndex;

class FunctionSymbol;
class ITRSProblem;

namespace TT {
typedef std::map<ExprSymbol, TT::Expression> Substitution;
typedef std::vector<TT::Expression> ExpressionVector;

class Relation;
class Addition;
class Subtraction;
class Multiplication;
class FunctionSymbol;
class GiNaCExpression;
class FunctionDefinition;
class Term;

class Expression {
public:
    Expression();
    Expression(const ITRSProblem &itrs, const GiNaC::ex &ex);
    Expression(const ITRSProblem &itrs, const FunctionSymbolIndex functionSymbol,
               const std::vector<Expression> &args);
    Expression(const Expression &ex);

    Expression& operator=(const Expression &r);
    Expression operator+(const Expression &r) const;
    Expression operator-(const Expression &r) const;
    Expression operator*(const Expression &r) const;

    void collectVariables(ExprSymbolSet &set) const;
    ExprSymbolSet getVariables() const;

    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;

    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;

    bool containsNoFunctionSymbols() const;

    Expression substitute(const Substitution &sub) const;
    Expression substitute(const GiNaC::exmap &sub) const;
    Expression evaluateFunction(const FunctionDefinition &funDef,
                                Expression &addToCost,
                                ExpressionVector &addToGuard) const;


    GiNaC::ex toGiNaC() const;
    Purrs::Expr toPurrs() const;
    Expression ginacify() const ;
    Expression unGinacify() const;

private:
    Expression(std::shared_ptr<Term> root);

private:
    std::shared_ptr<Term> root;

    std::shared_ptr<Term> getTermTree() const;

    friend std::ostream& operator<<(std::ostream &os, const Expression &ex);
    friend class Term;
    friend class FunctionSymbol;
    friend class GiNaCExpression;
};

std::ostream& operator<<(std::ostream &os, const Expression &ex);


// helper class for evaluateFunction()
class FunctionDefinition {
public:
    FunctionDefinition(const ::FunctionSymbol &fs,
                       const Expression &def,
                       const Expression &cost,
                       const ExpressionVector &guard);
    const ::FunctionSymbol& getFunctionSymbol() const;
    const Expression& getDefinition() const;
    const Expression& getCost() const;
    const ExpressionVector& getGuard() const;

private:
    const ::FunctionSymbol &functionSymbol;
    Expression definition;
    Expression cost;
    ExpressionVector guard;
};


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

    virtual void traverse(Visitor &visitor) = 0;
    virtual void traverse(ConstVisitor &visitor) const = 0;

    void collectVariables(ExprSymbolSet &set) const;
    ExprSymbolSet getVariables() const;

    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;

    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;

    bool containsNoFunctionSymbols() const;

    virtual std::shared_ptr<Term> copy() const = 0;
    virtual std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression &addToCost, ExpressionVector &addToGuard) const = 0;
    virtual std::shared_ptr<Term> substitute(const Substitution &sub) const = 0;
    virtual std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const = 0;

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
                                                   Expression &addToCost,
                                                   ExpressionVector &addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

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
                                                   Expression &addToCost,
                                                   ExpressionVector &addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

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
                                                   Expression &addToCost,
                                                   ExpressionVector &addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

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
                                                   Expression &addToCost,
                                                   ExpressionVector &addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

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
    FunctionSymbol(const ITRSProblem &itrs, FunctionSymbolIndex functionSymbol, const std::vector<Expression> &args);
    void traverse(Visitor &visitor) override;
    void traverse(ConstVisitor &visitor) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression &addToCost,
                                                   ExpressionVector &addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

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
                                                   Expression &addToCost,
                                                   ExpressionVector &addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

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