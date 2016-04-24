#include "term.h"

#include "itrs.h"

TermTree::~TermTree() {
}


void TermTree::collectVariables(std::set<VariableIndex> &set) const {
    class CollectingVisitor : public ConstVisitor {
    public:
        CollectingVisitor(std::set<VariableIndex> &set)
            : set(set) {
        }

        virtual void visitVariable(const VariableIndex &variable) {
            set.insert(variable);
        }

    private:
        std::set<VariableIndex> &set;
    };

    CollectingVisitor visitor(set);
    traverse(visitor);
}


std::set<VariableIndex> TermTree::getVariables() const {
    std::set<VariableIndex> set;
    collectVariables(set);
    return set;
}


void TermTree::collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const {
    class CollectingVisitor : public ConstVisitor {
    public:
        CollectingVisitor(std::set<FunctionSymbolIndex> &set)
            : set(set) {
        }

        virtual void visitFunctionSymbolPre(const FunctionSymbolIndex &functionSymbol) {
            set.insert(functionSymbol);
        }

    private:
        std::set<FunctionSymbolIndex> &set;
    };

    CollectingVisitor visitor(set);
    traverse(visitor);
}


void TermTree::collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const {
    class CollectingVisitor : public ConstVisitor {
    public:
        CollectingVisitor(std::vector<FunctionSymbolIndex> &vector)
            : vector(vector) {
        }

        virtual void visitFunctionSymbolPre(const FunctionSymbolIndex &functionSymbol) {
            vector.push_back(functionSymbol);
        }

    private:
        std::vector<FunctionSymbolIndex> &vector;
    };

    CollectingVisitor visitor(vector);
    traverse(visitor);
}


std::set<FunctionSymbolIndex> TermTree::getFunctionSymbols() const {
    std::set<FunctionSymbolIndex> set;
    collectFunctionSymbols(set);
    return set;
}


std::vector<FunctionSymbolIndex> TermTree::getFunctionSymbolsAsVector() const {
    std::vector<FunctionSymbolIndex> vector;
    collectFunctionSymbols(vector);
    return vector;
}


void TermTree::print(const ITRSProblem &itrs, std::ostream &os) const {
    class PrintVisitor : public TermTree::ConstVisitor {
    public:
        PrintVisitor(const ITRSProblem &itrs, std::ostream &os)
            : itrs(itrs), os(os) {
        }

        virtual void visitNumber(const GiNaC::numeric &value) {
            os << value;
        }
        virtual void visitAdditionPre() {
            os << "(";
        }
        virtual void visitAdditionIn() {
            os << " + ";
        }
        virtual void visitAdditionPost() {
            os << ")";
        }
        virtual void visitSubtractionPre() {
            os << "(";
        }
        virtual void visitSubtractionIn() {
            os << " - ";
        }
        virtual void visitSubtractionPost() {
            os << ")";
        }
        virtual void visitMultiplicationPre() {
            os << "(";
        }
        virtual void visitMultiplicationIn() {
            os << " * ";
        }
        virtual void visitMultiplicationPost() {
            os << ")";
        }
        virtual void visitFunctionSymbolPre(const FunctionSymbolIndex &functionSymbol) {
            os << itrs.getFunctionSymbolName(functionSymbol) << "(";
        }
        virtual void visitFunctionSymbolIn(const FunctionSymbolIndex &functionSymbol) {
            os << ", ";
        }
        virtual void visitFunctionSymbolPost(const FunctionSymbolIndex &functionSymbol) {
            os << ")";
        }
        virtual void visitVariable(const VariableIndex &variable) {
            os << itrs.getVarname(variable);
        }

    private:
        const ITRSProblem &itrs;
        std::ostream &os;
    };

    PrintVisitor printVisitor(itrs, os);
    traverse(printVisitor);
}


Number::Number(const GiNaC::numeric &value)
    : value(value) {
}


void Number::traverse(Visitor &visitor) {
    visitor.visitNumber(value);
}


void Number::traverse(ConstVisitor &visitor) const {
    visitor.visitNumber(value);
}


Addition::Addition(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r)
    : l(l), r(r) {
}


void Addition::traverse(Visitor &visitor) {
    visitor.visitAdditionPre();

    l->traverse(visitor);

    visitor.visitAdditionIn();

    r->traverse(visitor);

    visitor.visitAdditionPost();
}


void Addition::traverse(ConstVisitor &visitor) const {
    visitor.visitAdditionPre();

    l->traverse(visitor);

    visitor.visitAdditionIn();

    r->traverse(visitor);

    visitor.visitAdditionPost();
}


Subtraction::Subtraction(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r)
    : l(l), r(r) {
}


void Subtraction::traverse(Visitor &visitor) {
    visitor.visitSubtractionPre();

    l->traverse(visitor);

    visitor.visitSubtractionIn();

    r->traverse(visitor);

    visitor.visitSubtractionPost();
}


void Subtraction::traverse(ConstVisitor &visitor) const {
    visitor.visitSubtractionPre();

    l->traverse(visitor);

    visitor.visitSubtractionIn();

    r->traverse(visitor);

    visitor.visitSubtractionPost();
}


Multiplication::Multiplication(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r)
    : l(l), r(r) {
}


void Multiplication::traverse(Visitor &visitor) {
    visitor.visitMultiplicationPre();

    l->traverse(visitor);

    visitor.visitMultiplicationIn();

    r->traverse(visitor);

    visitor.visitMultiplicationPost();
}


void Multiplication::traverse(ConstVisitor &visitor) const {
    visitor.visitMultiplicationPre();

    l->traverse(visitor);

    visitor.visitMultiplicationIn();

    r->traverse(visitor);

    visitor.visitMultiplicationPost();
}


FunctionSymbol::FunctionSymbol(FunctionSymbolIndex functionSymbol, const std::vector<std::shared_ptr<TermTree>> &args)
    : functionSymbol(functionSymbol), args(args){
}


void FunctionSymbol::traverse(Visitor &visitor) {
    visitor.visitFunctionSymbolPre(functionSymbol);

    for (int i = 0; i + 1 < args.size(); ++i) {
        args[i]->traverse(visitor);
        visitor.visitFunctionSymbolIn(functionSymbol);
    }

    if (args.size() > 0) {
        args.back()->traverse(visitor);
    }
    visitor.visitFunctionSymbolPost(functionSymbol);
}


void FunctionSymbol::traverse(ConstVisitor &visitor) const {
    visitor.visitFunctionSymbolPre(functionSymbol);

    for (int i = 0; i + 1 < args.size(); ++i) {
        args[i]->traverse(visitor);
        visitor.visitFunctionSymbolIn(functionSymbol);
    }

    if (args.size() > 0) {
        args.back()->traverse(visitor);
    }
    visitor.visitFunctionSymbolPost(functionSymbol);
}


Variable::Variable(VariableIndex variable)
    : variable(variable) {
}


void Variable::traverse(Visitor &visitor) {
    visitor.visitVariable(variable);
}

void Variable::traverse(ConstVisitor &visitor) const {
    visitor.visitVariable(variable);
}
