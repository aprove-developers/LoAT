#ifndef ITRS_TERM_H
#define ITRS_TERM_H

#include <memory>
#include <ostream>
#include <set>
#include <utility>
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
typedef std::vector<std::pair<TT::Expression,ExprSymbol>> FunToVarSub;

class Relation;
class Addition;
class Subtraction;
class Multiplication;
class Power;
class Factorial;
class FunctionSymbol;
class GiNaCExpression;
class FunctionDefinition;
class Term;

class ConstVisitor;

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
    Factorial,
    FunctionSymbol,
    Number,
    Variable
};

class Expression {
public:
    Expression();
    Expression(const GiNaC::ex &ex);
    Expression(const FunctionSymbolIndex functionSymbol,
               const std::string &name,
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

    // simple expression =^= f(term_1, ..., term_n)
    // function symbol, no nested function symbols
    bool isSimple() const;

    void traverse(ConstVisitor &visitor) const;

    void collectVariables(ExprSymbolSet &set) const;
    ExprSymbolSet getVariables() const;

    void collectVariables(std::vector<ExprSymbol> &vector) const;
    std::vector<ExprSymbol> getVariablesAsVector() const;

    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;

    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;

    void collectUpdates(std::vector<Expression> &updates) const;
    std::vector<Expression> getUpdates() const;

    void collectFunctionApplications(std::vector<Expression> &app) const;
    std::vector<Expression> getFunctionApplications() const;

    bool hasFunctionSymbol(FunctionSymbolIndex funSym) const;
    bool hasFunctionSymbol() const;
    bool hasNoFunctionSymbols() const;
    // e.g., f(f(x)) has exactly one function symbol
    bool hasExactlyOneFunctionSymbol() const;

    bool hasExactlyOneFunctionSymbolOnce() const;

    Expression substitute(const Substitution &sub) const;
    Expression substitute(const GiNaC::exmap &sub) const;
    Expression substitute(FunctionSymbolIndex fs, const Expression &ex) const;
    Expression subsFirstFunAppByExpression(const Expression &ex) const;
    Expression evaluateFunction(const FunctionDefinition &funDef,
                                ::Expression *addToCost,
                                ExpressionVector *addToGuard) const;
    Expression evaluateFunction2(const FunctionDefinition &funDef,
                                 ::Expression *addToCost,
                                 ExpressionVector *addToGuard) const;
    Expression evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                       const TT::ExpressionVector &guard,
                                       ::Expression *addToCost,
                                       bool &modified) const;
    Expression moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard);
    Expression abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                            const std::map<FunctionSymbolIndex,int> &specialCases) const;


    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const;
    Purrs::Expr toPurrs(int i = -1) const;
    Expression ginacify() const ;
    Expression unGinacify() const;

    bool equals(const Expression &other) const;

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
    FunctionDefinition(ITRSProblem &itrs,
                       const FunctionSymbolIndex index,
                       const Expression &def,
                       const ::Expression &cost,
                       const ExpressionVector &guard);
    ITRSProblem& getITRSProblem() const;
    const FunctionSymbolIndex getFunctionSymbol() const;
    const Expression& getDefinition() const;
    const ::Expression& getCost() const;
    const ExpressionVector& getGuard() const;

private:
    ITRSProblem *itrs;
    FunctionSymbolIndex functionSymbol;
    Expression definition;
    ::Expression cost;
    ExpressionVector guard;
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

    virtual void visitPre(const Power &pow) {};
    virtual void visitIn(const Power &pow) {};
    virtual void visitPost(const Power &pow) {};

    virtual void visitPre(const Factorial &fact) {};
    virtual void visitPost(const Factorial &fact) {};

    virtual void visitPre(const FunctionSymbol &funcSym) {};
    virtual void visitIn(const FunctionSymbol &funcSym) {};
    virtual void visitPost(const FunctionSymbol &funcSym) {};

    virtual void visit(const GiNaCExpression &expr) {};
};


class Term {
public:
    EXCEPTION(UnsupportedExpressionException, CustomException);
    static std::shared_ptr<Term> fromGiNaC(const GiNaC::ex &ex);

public:
    virtual ~Term();

    virtual void traverse(ConstVisitor &visitor) const = 0;

    virtual int nops() const = 0;
    virtual std::shared_ptr<Term> op(int i) const = 0;

    bool has(const ExprSymbol &sym) const;

    void collectVariables(ExprSymbolSet &set) const;
    ExprSymbolSet getVariables() const;

    void collectVariables(std::vector<ExprSymbol> &vector) const;
    std::vector<ExprSymbol> getVariablesAsVector() const;

    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;

    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;

    void collectUpdates(std::vector<Expression> &updates) const;
    std::vector<Expression> getUpdates() const;

    void collectFunctionApplications(std::vector<Expression> &updates) const;
    std::vector<Expression> getFunctionApplications() const;

    bool hasFunctionSymbol(FunctionSymbolIndex funSym) const;
    bool hasFunctionSymbol() const;
    bool hasNoFunctionSymbols() const;
    bool hasExactlyOneFunctionSymbol() const;

    bool hasExactlyOneFunctionSymbolOnce() const;

    virtual bool info(InfoFlag info) const = 0;

    virtual std::shared_ptr<Term> copy() const = 0;
    virtual std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const = 0;
    virtual std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                                   ::Expression *addToCost,
                                                   ExpressionVector *addToGuard) const = 0;
    virtual std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                                    ::Expression *addToCost,
                                                    ExpressionVector *addToGuard) const = 0;
    virtual std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                          const TT::ExpressionVector &guard,
                                                          ::Expression *addToCost,
                                                          bool &modified) const = 0;
    virtual std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const = 0;
    virtual std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const;
    virtual std::shared_ptr<Term> substitute(const Substitution &sub) const = 0;
    virtual std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const = 0;
    virtual std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const = 0;

    EXCEPTION(UnsupportedOperationException, CustomException);
    virtual GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const;
    virtual Purrs::Expr toPurrs(int i) const;
    virtual std::shared_ptr<Term> ginacify() const = 0;
    virtual std::shared_ptr<Term> unGinacify() const = 0;

    bool equals(const std::shared_ptr<Term> other) const;
    virtual bool equals(const Term &term) const = 0; // use double dispatch
    virtual bool equals(const Relation &term) const;
    virtual bool equals(const Addition &term) const;
    virtual bool equals(const Subtraction &term) const;
    virtual bool equals(const Multiplication &term) const;
    virtual bool equals(const Power &term) const;
    virtual bool equals(const Factorial &term) const;
    virtual bool equals(const FunctionSymbol &term) const;
    virtual bool equals(const GiNaCExpression &term) const;
};

std::ostream& operator<<(std::ostream &os, const Term &term);


class Relation : public Term {
public:
    enum Type { EQUAL = 0, NOT_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL };
    static const char* TypeNames[];

public:
    Relation(Type type, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    Type getType() const;
    InfoFlag getTypeInfoFlag() const;

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const Relation &term) const override;

private:
    Type type;
    std::shared_ptr<Term> l, r;
};

class Addition : public Term {
public:
    Addition(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const Addition &term) const override;

private:
    std::shared_ptr<Term> l, r;
};


class Subtraction : public Term {
public:
    Subtraction(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const Subtraction &term) const override;

private:
    std::shared_ptr<Term> l, r;
};


class Multiplication : public Term {
public:
    Multiplication(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const Multiplication &term) const override;

private:
    std::shared_ptr<Term> l, r;
};


class Power : public Term {
public:
    Power(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const Power &term) const override;

private:
    std::shared_ptr<Term> l, r;
};


class Factorial : public Term {
public:
    Factorial(const std::shared_ptr<Term> &n);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const Factorial &term) const override;

private:
    std::shared_ptr<Term> n;
};


class FunctionSymbol : public Term {
public:
    FunctionSymbol(FunctionSymbolIndex functionSymbol, const std::string &name, const std::vector<std::shared_ptr<Term>> &args);
    FunctionSymbol(FunctionSymbolIndex functionSymbol, const std::string &name, const std::vector<Expression> &args);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    FunctionSymbolIndex getFunctionSymbol() const;
    std::string getName() const;
    const std::vector<std::shared_ptr<Term>>& getArguments() const;

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const FunctionSymbol &term) const override;

private:
    FunctionSymbolIndex functionSymbol;
    std::string name;
    std::vector<std::shared_ptr<Term>> args;
};


class GiNaCExpression : public Term {
public:
    GiNaCExpression(const GiNaC::ex &expr);
    void traverse(ConstVisitor &visitor) const override;

    int nops() const override;
    std::shared_ptr<Term> op(int i) const override;

    bool info(InfoFlag info) const override;

    std::shared_ptr<Term> copy() const override;
    std::shared_ptr<Term> subsFirstFunAppByExpression(const std::shared_ptr<Term> ex, bool &already) const override;
    std::shared_ptr<Term> evaluateFunction(const FunctionDefinition &funDef,
                                           ::Expression *addToCost,
                                           ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunction2(const FunctionDefinition &funDef,
                                            ::Expression *addToCost,
                                            ExpressionVector *addToGuard) const override;
    std::shared_ptr<Term> evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                  const TT::ExpressionVector &guard,
                                                  ::Expression *addToCost,
                                                  bool &modified) const override;
    std::shared_ptr<Term> moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) const override;
    std::shared_ptr<Term> abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                               const std::map<FunctionSymbolIndex,int> &specialCases) const override;
    std::shared_ptr<Term> substitute(const Substitution &sub) const override;
    std::shared_ptr<Term> substitute(const GiNaC::exmap &sub) const override;
    std::shared_ptr<Term> substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const override;

    GiNaC::ex getExpression() const;
    void setExpression(const GiNaC::ex &expr);

    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const override;
    Purrs::Expr toPurrs(int i) const override;
    std::shared_ptr<Term> ginacify() const override;
    std::shared_ptr<Term> unGinacify() const override;

    bool equals(const Term &term) const override;
    bool equals(const GiNaCExpression &term) const override;

private:
    GiNaC::ex expression;
};

};

#endif // ITRS_TERM_H