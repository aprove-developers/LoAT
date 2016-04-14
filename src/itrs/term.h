#ifndef ITRS_TERM_H
#define ITRS_TERM_H

#include <memory>
#include <set>

#include <ginac/ginac.h>

// typedefs for readability
typedef int FunctionSymbolIndex;
typedef int VariableIndex;

namespace ITRS {

/**
 * Represents a term (lhs/rhs) of a rule as an AST.
 */
class TermTree {
public:
    class Visitor {
    public:
        virtual void visitNumber(const GiNaC::numeric &value) {};
        virtual void visitAdditionPre() {};
        virtual void visitAdditionIn() {};
        virtual void visitAdditionPost() {};
        virtual void visitSubtractionPre() {};
        virtual void visitSubtractionIn() {};
        virtual void visitSubtractionPost() {};
        virtual void visitMultiplicationPre() {};
        virtual void visitMultiplicationIn() {};
        virtual void visitMultiplicationPost() {};
        virtual void visitFunctionSymbolPre(FunctionSymbolIndex &functionSymbol) {};
        virtual void visitFunctionSymbolIn(FunctionSymbolIndex &functionSymbol) {};
        virtual void visitFunctionSymbolPost(FunctionSymbolIndex &functionSymbol) {};
        virtual void visitVariable(VariableIndex &variable) {};
    };

    class ConstVisitor {
    public:
        virtual void visitNumber(const GiNaC::numeric &value) {};
        virtual void visitAdditionPre() {};
        virtual void visitAdditionIn() {};
        virtual void visitAdditionPost() {};
        virtual void visitSubtractionPre() {};
        virtual void visitSubtractionIn() {};
        virtual void visitSubtractionPost() {};
        virtual void visitMultiplicationPre() {};
        virtual void visitMultiplicationIn() {};
        virtual void visitMultiplicationPost() {};
        virtual void visitFunctionSymbolPre(const FunctionSymbolIndex &functionSymbol) {};
        virtual void visitFunctionSymbolIn(const FunctionSymbolIndex &functionSymbol) {};
        virtual void visitFunctionSymbolPost(const FunctionSymbolIndex &functionSymbol) {};
        virtual void visitVariable(const VariableIndex &variable) {};
    };

public:
    virtual ~TermTree();

    void collectVariables(std::set<VariableIndex> &set) const;
    std::set<VariableIndex> getVariables() const;

    virtual void traverse(Visitor &visitor) = 0;
    virtual void traverse(ConstVisitor &visitor) const = 0;
};



class PrintVisitor : public TermTree::ConstVisitor {
public:
    PrintVisitor(const std::vector<std::string> &vars,
                 const std::vector<std::string> &funcs)
        : vars(vars), funcs(funcs) {
    }

    virtual void visitNumber(const GiNaC::numeric &value) {
        std::cout << value;
    }
    virtual void visitAdditionPre() {
        std::cout << "(";
    }
    virtual void visitAdditionIn() {
        std::cout << " + ";
    }
    virtual void visitAdditionPost() {
        std::cout << ")";
    }
    virtual void visitSubtractionPre() {
        std::cout << "(";
    }
    virtual void visitSubtractionIn() {
        std::cout << " - ";
    }
    virtual void visitSubtractionPost() {
        std::cout << ")";
    }
    virtual void visitMultiplicationPre() {
        std::cout << "(";
    }
    virtual void visitMultiplicationIn() {
        std::cout << " * ";
    }
    virtual void visitMultiplicationPost() {
        std::cout << ")";
    }
    virtual void visitFunctionSymbolPre(const FunctionSymbolIndex &functionSymbol) {
        std::cout << funcs[functionSymbol] << "(";
    }
    virtual void visitFunctionSymbolIn(const FunctionSymbolIndex &functionSymbol) {
        std::cout << ", ";
    }
    virtual void visitFunctionSymbolPost(const FunctionSymbolIndex &functionSymbol) {
        std::cout << ")";
    }
    virtual void visitVariable(const VariableIndex &variable) {
        std::cout << vars[variable];
    }

private:
    const std::vector<std::string> &vars;
    const std::vector<std::string> &funcs;
};


class Number : public TermTree {
public:
    Number(const GiNaC::numeric &value);
    virtual void traverse(Visitor &visitor);
    virtual void traverse(ConstVisitor &visitor) const;

private:
    GiNaC::numeric value;
};


class Addition : public TermTree {
public:
    Addition(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r);
    virtual void traverse(Visitor &visitor);
    virtual void traverse(ConstVisitor &visitor) const;

private:
    std::shared_ptr<TermTree> l, r;
};


class Subtraction : public TermTree {
public:
    Subtraction(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r);
    virtual void traverse(Visitor &visitor);
    virtual void traverse(ConstVisitor &visitor) const;

private:
    std::shared_ptr<TermTree> l, r;
};


class Multiplication : public TermTree {
public:
    Multiplication(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r);
    virtual void traverse(Visitor &visitor);
    virtual void traverse(ConstVisitor &visitor) const;

private:
    std::shared_ptr<TermTree> l, r;
};


class FunctionSymbol : public TermTree {
public:
    FunctionSymbol(FunctionSymbolIndex functionSymbol, const std::vector<std::shared_ptr<TermTree>> &args);
    virtual void traverse(Visitor &visitor);
    virtual void traverse(ConstVisitor &visitor) const;

private:
    FunctionSymbolIndex functionSymbol;
    std::vector<std::shared_ptr<TermTree>> args;
};


class Variable : public TermTree {
public:
    Variable(VariableIndex variable);
    virtual void traverse(Visitor &visitor);
    virtual void traverse(ConstVisitor &visitor) const;

private:
    VariableIndex variable;
};

} // namespace ITRS

#endif // ITRS_TERM_H