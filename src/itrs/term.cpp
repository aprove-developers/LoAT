#include "term.h"

namespace ITRS {

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

} // namespace ITRS