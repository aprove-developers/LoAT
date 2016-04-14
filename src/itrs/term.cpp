#include "term.h"

namespace ITRS {

TermTree::~TermTree() {
}


void TermTree::Visitor::visitNumber(const GiNaC::numeric &value) {
}


void TermTree::Visitor::visitAdditionPre() {
}


void TermTree::Visitor::visitAdditionIn() {
}


void TermTree::Visitor::visitAdditionPost() {
}


void TermTree::Visitor::visitSubtractionPre() {
}


void TermTree::Visitor::visitSubtractionIn() {
}


void TermTree::Visitor::visitSubtractionPost() {
}


void TermTree::Visitor::visitMultiplicationPre() {
}


void TermTree::Visitor::visitMultiplicationIn() {
}


void TermTree::Visitor::visitMultiplicationPost() {
}


void TermTree::Visitor::visitFunctionSymbolPre(FunctionSymbolIndex functionSymbol) {
}


void TermTree::Visitor::visitFunctionSymbolIn(FunctionSymbolIndex functionSymbol) {
}


void TermTree::Visitor::visitFunctionSymbolPost(FunctionSymbolIndex functionSymbol) {
}


void TermTree::Visitor::visitVariable(VariableIndex variable) {
}


Number::Number(const GiNaC::numeric &value)
    : value(value) {
}


void Number::traverse(Visitor &visitor) const {
    visitor.visitNumber(value);
}


Addition::Addition(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r)
    : l(l), r(r) {
}


void Addition::traverse(Visitor &visitor) const {
    visitor.visitAdditionPre();

    l->traverse(visitor);

    visitor.visitAdditionIn();

    r->traverse(visitor);

    visitor.visitAdditionPost();
}


Subtraction::Subtraction(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r)
    : l(l), r(r) {
}


void Subtraction::traverse(Visitor &visitor) const {
    visitor.visitSubtractionPre();

    l->traverse(visitor);

    visitor.visitSubtractionIn();

    r->traverse(visitor);

    visitor.visitSubtractionPost();
}


Multiplication::Multiplication(const std::shared_ptr<TermTree> &l, const std::shared_ptr<TermTree> &r)
    : l(l), r(r) {
}


void Multiplication::traverse(Visitor &visitor) const {
    visitor.visitMultiplicationPre();

    l->traverse(visitor);

    visitor.visitMultiplicationIn();

    r->traverse(visitor);

    visitor.visitMultiplicationPost();
}


FunctionSymbol::FunctionSymbol(FunctionSymbolIndex functionSymbol, const std::vector<std::shared_ptr<TermTree>> &args)
    : functionSymbol(functionSymbol), args(args){
}


void FunctionSymbol::traverse(Visitor &visitor) const {
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


void Variable::traverse(Visitor &visitor) const {
    visitor.visitVariable(variable);
}

} // namespace ITRS