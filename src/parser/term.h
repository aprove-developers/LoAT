#ifndef PARSER_TERM_H
#define PARSER_TERM_H

#include <memory>
#include <vector>

#include "expression.h"
#include "its/itsproblem.h"


namespace parser {

// typedef for readability
class Term;
typedef std::shared_ptr<Term> TermPtr;


/*
 * Represents a parsed term, consisting of function applications, arithmetic and variables.
 */
class Term {
public:
    EXCEPTION(CannotConvertToGinacException, CustomException);

    enum TermType {
        BinaryOperation,
        FunctionApplication,
        Variable,
        Number
    };

public:
    virtual ~Term() {}

    // Returns the type of this term
    virtual TermType getType() const = 0;

    // Returns true iff this term does not contain any function symbols
    virtual bool isArithmeticExpression() const = 0;

    // Returns true iff this term is a function application whose arguments are arithmetic expressions
    virtual bool isFunappOnArithmeticExpressions() const = 0;

    // Collects all variables that occur somewhere in this term to the given set
    virtual void collectVariables(std::set<VariableIdx> &set) const = 0;

    // Translates this term into a ginac expression
    // The ITSProblem instance is used to map VariableIdx to ginac symbols
    virtual Expression toGinacExpression(const ITSProblem &its) const = 0;
};


/**
 * Represents a binary arithmetic operation.
 */
class TermBinOp : public Term {
public:
    enum Operation {
        Addition,
        Subtraction,
        Multiplication,
        Division,
        Power
    };

    TermBinOp(const TermPtr &l, const TermPtr &r, Operation type) : lhs(l), rhs(r), op(type) {}
    TermType getType() const override { return Term::BinaryOperation; }

    Operation getOperation() const { return op; }
    TermPtr getLhs() { return lhs; }
    TermPtr getRhs() { return rhs; }

    bool isArithmeticExpression() const override;
    bool isFunappOnArithmeticExpressions() const override { return false; }

    void collectVariables(std::set<VariableIdx> &set) const override;
    Expression toGinacExpression(const ITSProblem &its) const override;

private:
    TermPtr lhs, rhs;
    Operation op;
};


/**
 * Represents a function application. The function symbol is stored as string.
 */
class TermFunApp : public Term {
public:
    TermFunApp(std::string functionSymbol, const std::vector<TermPtr> &args) : name(functionSymbol), args(args) {}
    TermType getType() const override { return Term::FunctionApplication; }

    std::string getName() const { return name; }
    int getArity() const { return args.size(); }
    const std::vector<std::shared_ptr<Term>>& getArguments() const { return args; }

    bool isArithmeticExpression() const override { return false; }
    bool isFunappOnArithmeticExpressions() const override;
    void collectVariables(std::set<VariableIdx> &set) const override;
    Expression toGinacExpression(const ITSProblem &its) const override;

private:
    std::string name;
    std::vector<std::shared_ptr<Term>> args;
};


/**
 * Represents a variable, which is stored as VariableIdx.
 */
class TermVariable : public Term {
public:
    TermVariable(VariableIdx variableIdx) : var(variableIdx) {}
    TermType getType() const override { return Term::Variable; }

    VariableIdx getVariableIdx() const { return var; }

    bool isArithmeticExpression() const override { return true; }
    bool isFunappOnArithmeticExpressions() const override { return false; }

    void collectVariables(std::set<VariableIdx> &set) const override { set.insert(var); }
    Expression toGinacExpression(const ITSProblem &its) const override { return its.getGinacSymbol(var); }

private:
    VariableIdx var;
};


/**
 * Represents any kind of number, which is stored as GiNaC::numeric.
 */
class TermNumber : public Term {
public:
    TermNumber(GiNaC::numeric number) : num(number) {}
    TermType getType() const override { return Term::Number; }

    GiNaC::numeric getNumber() const { return num; }

    bool isArithmeticExpression() const override { return true; }
    bool isFunappOnArithmeticExpressions() const override { return false; }

    void collectVariables(std::set<VariableIdx> &) const override {}
    Expression toGinacExpression(const ITSProblem &) const override { return num; }

private:
    GiNaC::numeric num;
};


/**
 * A class to represent a relation consisting of two terms and a relational operator.
 */
class Relation {
public:
    enum Operator {
        RelationEqual,
        RelationNotEqual,
        RelationGreater,
        RelationGreaterEqual,
        RelationLess,
        RelationLessEqual
    };

    Relation(TermPtr lhs, TermPtr rhs, Operator type) : lhs(lhs), rhs(rhs), op(type) {}

    TermPtr getLhs() const { return lhs; }
    TermPtr getRhs() const { return rhs; }
    Operator getOperator() const { return op; }

    Expression toGinacExpression(const ITSProblem &its) const;

private:
    TermPtr lhs, rhs;
    Operator op;
};

}

#endif //PARSER_TERM_H
