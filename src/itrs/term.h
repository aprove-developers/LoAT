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

// typedefs for readability
typedef int FunctionSymbolIndex;
typedef int VariableIndex;

class FunctionSymbol;
class ITRSProblem;

namespace TT {
class Expression;

typedef std::map<ExprSymbol,TT::Expression,GiNaC::ex_is_less> Substitution;
typedef std::vector<TT::Expression> ExpressionVector;

class Relation;
class Addition;
class Subtraction;
class Multiplication;
class Power;
class FunctionSymbol;
class GiNaCExpression;
class FunctionDefinition;
class Term;

enum class InfoFlag {
    RelationEqual,
    RelationNotEqual,
    RelationGreater,
    RelationGreaterEqual,
    RelationLess,
    RelationLessEqual,
    Relation,
    Addition,
    Subtraction,
    Multiplication,
    Power,
    FunctionSymbol,
    Number
};

class Expression {
public:
    Expression();
    Expression(const ITRSProblem &itrs, const GiNaC::ex &ex);
    Expression(const ITRSProblem &itrs, const FunctionSymbolIndex functionSymbol,
               const std::vector<Expression> &args);
    // copy constructor and assignment operator
    Expression(const Expression &ex);
    Expression& operator=(const Expression &r);

    // move constructor and move assignment operator
    Expression(Expression &&ex);
    Expression& operator=(Expression &&r);

    Expression operator+(const GiNaC::ex &rhs) const;
    Expression operator-(const GiNaC::ex &rhs) const;
    Expression operator*(const GiNaC::ex &rhs) const;
    Expression operator^(const GiNaC::ex &rhs) const;

    Expression operator==(const GiNaC::ex &rhs) const;
    Expression operator!=(const GiNaC::ex &rhs) const;
    Expression operator<(const GiNaC::ex &rhs) const;
    Expression operator<=(const GiNaC::ex &rhs) const;
    Expression operator>(const GiNaC::ex &rhs) const;
    Expression operator>=(const GiNaC::ex &rhs) const;

    Expression operator+(const Expression &rhs) const;
    Expression operator-(const Expression &rhs) const;
    Expression operator*(const Expression &rhs) const;
    Expression operator^(const Expression &rhs) const;

    Expression operator==(const Expression &rhs) const;
    Expression operator!=(const Expression &rhs) const;
    Expression operator<(const Expression &rhs) const;
    Expression operator<=(const Expression &rhs) const;
    Expression operator>(const Expression &rhs) const;
    Expression operator>=(const Expression &rhs) const;

    Expression& operator+=(const Expression &rhs);
    Expression& operator-=(const Expression &rhs);
    Expression& operator*=(const Expression &rhs);
    Expression& operator^=(const Expression &rhs);

    int nops() const;
    Expression op(int i) const;

    bool info(InfoFlag info) const;

    bool has(const ExprSymbol &sym) const;

    // simple expression =^= f(term1, .., term2)
    // function symbol, no nested function symbols
    bool isSimple() const;

    void collectVariables(ExprSymbolSet &set) const;
    ExprSymbolSet getVariables() const;

    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;

    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;

    void collectUpdates(std::vector<Expression> &updates) const;
    std::vector<Expression> getUpdates() const;

    void collectFunctionApplications(std::vector<Expression> &app) const;
    std::vector<Expression> getFunctionApplications() const;

    bool hasNoFunctionSymbols() const;
    bool hasExactlyOneFunctionSymbol() const;

    Expression substitute(const Substitution &sub) const;
    Expression substitute(const GiNaC::exmap &sub) const;
    Expression evaluateFunction(const FunctionDefinition &funDef,
                                Expression *addToCost,
                                ExpressionVector *addToGuard) const;


    GiNaC::ex toGiNaC(bool subFunSyms = false) const;
    Purrs::Expr toPurrs(int i = -1) const;
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
    FunctionDefinition(const FunctionSymbolIndex fs,
                       const Expression &def,
                       const Expression &cost,
                       const ExpressionVector &guard);
    const FunctionSymbolIndex getFunctionSymbol() const;
    const Expression& getDefinition() const;
    const Expression& getCost() const;
    const ExpressionVector& getGuard() const;

private:
    FunctionSymbolIndex functionSymbol;
    Expression definition;
    Expression cost;
    ExpressionVector guard;
};


class Term {
public:
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

        virtual void visitPre(const Power &pow) {};
        virtual void visitIn(const Power &pow) {};
        virtual void visitPost(const Power &pow) {};

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

    virtual void traverse(ConstVisitor &visitor) const = 0;

    virtual int nops() const = 0;
    virtual std::shared_ptr<Term> op(int i) const = 0;

    bool has(const ExprSymbol &sym) const;

    void collectVariables(ExprSymbolSet &set) const;
    ExprSymbolSet getVariables() const;

    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;

    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;

    void collectUpdates(std::vector<Expression> &updates) const;
    std::vector<Expression> getUpdates() const;

    void collectFunctionApplications(std::vector<Expression> &updates) const;
    std::vector<Expression> getFunctionApplications() const;

    bool hasNoFunctionSymbols() const;
    bool hasExactlyOneFunctionSymbol() const;

    virtual bool info(InfoFlag info) const = 0;

    virtual std::shared_ptr<Term> copy() const = 0;
    virtual std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const = 0;
    virtual std::shared_ptr<Term> substitute(const Substitution &sub) const = 0;
    virtual std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const = 0;

    EXCEPTION(UnsupportedOperationException, CustomException);
    virtual GiNaC::ex toGiNaC(bool subFunSyms = false) const;
    virtual Purrs::Expr toPurrs(int i) const;
    virtual std::shared_ptr<Term> ginacify() const = 0;
    virtual std::shared_ptr<Term> unGinacify() const = 0;

private:
    const ITRSProblem &itrs;
};

std::ostream& operator<<(std::ostream &os, const Term &term);


class Relation : public Term {
public:
    enum Type { EQUAL = 0, NOT_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL };
    static const char* TypeNames[];

public:
    Relation(const ITRSProblem &itrs, Type type, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

    Type getType() const;
    InfoFlag getTypeInfoFlag() const;

    GiNaC::ex toGiNaC(bool subFunSyms = false) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    Type type;
    std::shared_ptr<Term> l, r;
};

class Addition : public Term {
public:
    Addition(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    std::shared_ptr<Term> l, r;
};


class Subtraction : public Term {
public:
    Subtraction(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    std::shared_ptr<Term> l, r;
};


class Multiplication : public Term {
public:
    Multiplication(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    std::shared_ptr<Term> l, r;
};


class Power : public Term {
public:
    Power(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    std::shared_ptr<Term> l, r;
};


class FunctionSymbol : public Term {
public:
    FunctionSymbol(const ITRSProblem &itrs, FunctionSymbolIndex functionSymbol, const std::vector<std::shared_ptr<Term>> &args);
    FunctionSymbol(const ITRSProblem &itrs, FunctionSymbolIndex functionSymbol, const std::vector<Expression> &args);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

    FunctionSymbolIndex getFunctionSymbol() const;
    const std::vector<std::shared_ptr<Term>>& getArguments() const;

    GiNaC::ex toGiNaC(bool subFunSyms = false) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    FunctionSymbolIndex functionSymbol;
    std::vector<std::shared_ptr<Term>> args;
};


class GiNaCExpression : public Term {
public:
    GiNaCExpression(const ITRSProblem &itrs, const GiNaC::ex &expr);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;

    GiNaC::ex getExpression() const;
    void setExpression(const GiNaC::ex &expr);

    GiNaC::ex toGiNaC(bool subFunSyms = false) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

private:
    GiNaC::ex expression;
};

};

#endif // ITRS_TERM_H