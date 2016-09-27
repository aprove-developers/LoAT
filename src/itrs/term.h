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

// more typedefs for readability
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



/*
 * This enum is used for the info() method to check what the expression represents.
 */
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


/*
 * This class represents a term that is built from both
 * program function symbols and theory function symbols.
 * Subterms that consist only of theory function symbols
 * are (usually, not necessarily) represented by GiNaC expressions.
 * This basically a wrapper for a smart pointer to a "Term" object.
 */
class Expression {
public:
    Expression();

    /*
     * Creates an Expression from a GiNaC expression.
     * @param ex the GiNaC expression
     */
    Expression(const GiNaC::ex &ex);

    /*
     * Creates an Expression representing a function symbol.
     * @param name the function symbol's name
     * @param args the arguments
     */
    Expression(const FunctionSymbolIndex functionSymbol,
               const std::string &name,
               const std::vector<Expression> &args);

    /*
     * Copy constructor.
     * @param ex the Expression to create the copy from
     * @return the resulting Expression
     */
    Expression(const Expression &ex);

    /*
     * Assignment operator.
     * @param r the Expression to create the copy from
     * @return reference to the resulting Expression
     */
    Expression& operator=(const Expression &r);

    /*
     * Move constructor.
     * @param ex the Expression that should be moved
     * @return the resulting Expression
     */
    Expression(Expression &&ex);

    /*
     * Assignment move operator.
     * @param r the Expression that should be moved
     * @return reference to the resulting Expression
     */
    Expression& operator=(Expression &&r);


    /*
     * Operators overloaded for convenience.
     * @param rhs the right-hand side (GiNaC expression)
     * @return the resulting Expression
     */
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

    /*
     * Operators overloaded for convenience.
     * @param rhs the right-hand side (Expression)
     * @return the resulting Expression
     */
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

    /*
     * The number of arguments.
     * For function symbols, this is the arity.
     * For GiNaC expressions, this is forwarded to the GiNaC expression.
     * Otherwise, this is 2 or 1.
     * @return the number of arguments
     */
    int nops() const;

    /*
     * Get the ith argument.
     * @param i a number with 0 <= i < nops()
     * @return the ith argument
     */
    Expression op(int i) const;

    /*
     * Get information about this Expression.
     * @return true iff the Expression satisfies the flag
     */
    bool info(InfoFlag info) const;

    /*
     * Check whether the given variable occurs in this Expression.
     * @return true iff the variable occurs in this Expression.
     */
    bool has(const ExprSymbol &sym) const;

    /*
     * Check whether this Expression is "simple",
     * i.e., whether it is of the form f(term_1, ..., term_n)
     *       and does not contain nested function symbols.
     * @return true iff the variable occurs in this Expression.
     */
    bool isSimple() const;

    /*
     * Let the given visitor traverse the tree.
     * @return visitor a visitor
     */
    void traverse(ConstVisitor &visitor) const;

    /*
     * Collect all variables of the Expression.
     * @param set the set to collect the variables in
     */
    void collectVariables(ExprSymbolSet &set) const;

    /*
     * Return a set of all variables in this Expression.
     * @return a set of all variables in this Expression
     */
    ExprSymbolSet getVariables() const;

    /*
     * Collect all variables of the Expression in a vector.
     * @param vector the vector to collect the variables in
     */
    void collectVariables(std::vector<ExprSymbol> &vector) const;

    /*
     * Return a vector of all variables in this Expression.
     * @return a vector of all variables in this Expression
     */
    std::vector<ExprSymbol> getVariablesAsVector() const;

    /*
     * Collect all function symbols of the Expression.
     * @param set the set to collect the function symbols in
     */
    void collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const;

    /*
     * Return a set of all function symbols in this Expression.
     * @return a set of all function symbols in this Expression
     */
    std::set<FunctionSymbolIndex> getFunctionSymbols() const;

    /*
     * Collect all function symbols of the Expression in a vector.
     * @param vector the vector to collect the function symbols in
     */
    void collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const;

    /*
     * Return a vector of all function symbols in this Expression.
     * @return a vector of all function symbols in this Expression
     */
    std::vector<FunctionSymbolIndex> getFunctionSymbolsAsVector() const;


    /*
     * Collect all updates,
     * i.e., terms that are direct subterms of a function symbol,
     * of the Expression in a vector.
     * @param updates the vector to collect the updates in
     */
    void collectUpdates(std::vector<Expression> &updates) const;

    /*
     * Return a vector of all updates,
     * i.e., terms that are direct subterms of a function symbol,
     * in this Expression.
     * @return a vector of all updates in this Expression
     */
    std::vector<Expression> getUpdates() const;

    /*
     * Collect all function applications,
     * i.e., terms of the form f(t_1, ..., t_n) where f is a program fs,
     * of the Expression in a vector.
     * @param updates the vector to collect the function applications in
     */
    void collectFunctionApplications(std::vector<Expression> &app) const;

    /*
     * Return a vector of all function applications,
     * i.e., terms of the form f(t_1, ..., t_n) where f is a program fs,
     * in this Expression.
     * @return a vector of all function applications in this Expression
     */
    std::vector<Expression> getFunctionApplications() const;

    /*
     * Check if the given function symbol occurs in this Expression.
     * @param funSym a function symbol
     * @return true iff the function symbol occurs in this Expression
     */
    bool hasFunctionSymbol(FunctionSymbolIndex funSym) const;

    /*
     * Check if this Expression has any program function symbols.
     * @return true iff this Expression has any program function symbols
     */
    bool hasFunctionSymbol() const;

    /*
     * Check if this Expression does not have any program function symbols.
     * @return true iff this Expression does not have any program function symbols
     */
    bool hasNoFunctionSymbols() const;

    /*
     * Check if this Expression has exactly one distinct function symbol,
     * e.g., f(f(x)) has only one distinct function symbol.
     * @return true iff this Expression has any program function symbols
     */
    bool hasExactlyOneFunctionSymbol() const;

    /*
     * Check if this Expression has exactly one distinct function symbol
     * and if this function symbols occurs exactly once.
     * @return true iff this Expression has exactly one distinct function symbol
     *                  and it occurs exactly once
     */
    bool hasExactlyOneFunctionSymbolOnce() const;

    /*
     * Apply the given substitution.
     * IMPORTANT: Call unGinacify() first!
     * @param sub the substitution to apply
     * @return the Expression resulting from the substitution
     */
    Expression substitute(const Substitution &sub) const;

    /*
     * Apply the given (GiNaC) substitution.
     * @param sub the substitution to apply
     * @return the Expression resulting from the substitution
     */
    Expression substitute(const GiNaC::exmap &sub) const;

    /*
     * Substitute all occurences of the given function symbol by the given Expression.
     * @param fs the function symbol to substitute
     * @param ex the Expression to substitute the function symbol by
     * @return the Expression resulting from the substitution
     */
    Expression substitute(FunctionSymbolIndex fs, const Expression &ex) const;

    /*
     * Substitute the first function symbol by the given term.
     * Here, "first" means the function symbol that is reached first
     * while traversing the Expression.
     * The Expression is traversed in the same order as a visitor traverses it
     * and therefore in the same order as the collect() functions do.
     * @param ex the Expression to substitute the function symbol by
     * @return the Expression resulting from the substitution
     */
    Expression subsFirstFunAppByExpression(const Expression &ex) const;

    /*
     * Evaluate function calls using the given function definition.
     * The cost caused by this is added to "addToCost" (if this is not nullptr)
     * and the constraints that come up are added to "addToGuard" (if this is not nullptr).
     * @param funDef the function definition used for the evaluation
     * @param addToCost the Expression to add all caused cost to
     * @param addToGuard the vector to add call constraints to
     * @return the resulting Expression
     */
    Expression evaluateFunction(const FunctionDefinition &funDef,
                                ::Expression *addToCost,
                                ExpressionVector *addToGuard) const;

    /*
     * Evaluate function calls using the given function definition.
     * Here, arguments that contain program function symbols are moved to the guard.
     * The cost caused by this is added to "addToCost" (if this is not nullptr)
     * and the constraints that come up are added to "addToGuard" (if this is not nullptr).
     * @param funDef the function definition used for the evaluation
     * @param addToCost the Expression to add all caused cost to
     * @param addToGuard the vector to add call constraints to
     * @return the resulting Expression
     */
    Expression evaluateFunction2(const FunctionDefinition &funDef,
                                 ::Expression *addToCost,
                                 ExpressionVector *addToGuard) const;

    /*
     * Evaluate specific recursive calls, i.e., calls where only values are passed,
     * if these arguments satisfy the guard.
     * This tries to perform symbolic execution by using the supplied guard.
     * The cost caused by this is added to "addToCost" (if this is not nullptr).
     * @param funDef the function definition used for the evaluation
     * @param guard constraints that already hold
     * @param addToCost the Expression to add all caused cost to
     * @param modified is set to true if a call was evaluated
     * @return the resulting Expression
     */
    Expression evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                       const TT::ExpressionVector &guard,
                                       ::Expression *addToCost,
                                       bool &modified) const;

    /*
     * Move all recursive calls to the given guard.
     * @param itrs the ITRSProblem
     * @param guard the guard where the function symbols should be moved to
     * @return the resulting Expression
     */
    Expression moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard);

    // a function that is used in itrsToLoAT
    Expression abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                            const std::map<FunctionSymbolIndex,int> &specialCases) const;

    /*
     * Returns this Expression as a GiNaC expression.
     * @param subFunSyms if this is true then program function symbols are
     *                   substituted by fresh variables
     * @param the other arguments were used at some point and should probably be removed
     * @return the resulting GiNaC expression
     */
    GiNaC::ex toGiNaC(bool subFunSyms = false, const ExprSymbol *singleVar = nullptr, FunToVarSub *sub = nullptr) const;

    /*
     * Returns this Expression as a PURRS expression.
     * @param i the ith argument is the argument that is passed to the x(n) function
     * @return the resulting PURRS expression
     */
    Purrs::Expr toPurrs(int i = -1) const;

    /*
     * "GiNaCiFiEs" this Expression, i.e., replaces subterms
     * that consist only of theory function symbols by GiNaC Expressions.
     * @return the ginacified Expression
     */
    Expression ginacify() const ;

    /*
     * "UnGiNaCiFiEs" this Expression, i.e., in the resulting Expression,
     * only variables and numbers are represented by GiNaC expressions.
     * @return the ginacified Expression
     */
    Expression unGinacify() const;

    /*
     * Checks whether this Expression is syntactically equal to the given Expression.
     * @param other the Expression to compare this one with
     * @return true iff this Expression is syntactically equal to the given Expression
     */
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


/*
 * This is a helper class for evaluateFunction()
 * and is used to encapsulate a function definition.
 * This is basically a RightHandSide with the ITRSProblem
 * and the function symbol index.
 */
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


/*
 * A visitor class.
 * visitPre: called when first encountering a term
 * visitIn: called between visits to the terms arguments
 * visitPost: called after encountering a term
 */
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


/*
 * This is the actual class used to represent terms that contain both
 * program function symbols and theory function symbols.
 */
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