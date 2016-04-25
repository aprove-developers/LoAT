#include "term.h"

#include "itrs.h"

#include "expression.h"

namespace TT {

Term::Term(const ITRSProblem &itrs)
    : itrs(itrs) {
}


Term::~Term() {
}


const ITRSProblem& Term::getITRSProblem() const {
    return itrs;
}


void Term::collectVariables(ExprSymbolSet &set) const {
    class CollectingVisitor : public ConstVisitor {
    public:
        CollectingVisitor(ExprSymbolSet &set)
            : set(set) {
        }

        void visit(const GiNaCExpression &expr) override {
            Expression customExpr(expr.getExpression());
            customExpr.collectVariables(set);
        }

    private:
        ExprSymbolSet &set;
    };

    CollectingVisitor visitor(set);
    traverse(visitor);
}


ExprSymbolSet Term::getVariables() const {
    ExprSymbolSet set;
    collectVariables(set);
    return set;
}


void Term::collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const {
    class CollectingVisitor : public ConstVisitor {
    public:
        CollectingVisitor(std::set<FunctionSymbolIndex> &set)
            : set(set) {
        }

        void visitPre(const FunctionSymbol &funSymbol) override {
            set.insert(funSymbol.getFunctionSymbol());
        }

    private:
        std::set<FunctionSymbolIndex> &set;
    };

    CollectingVisitor visitor(set);
    traverse(visitor);
}


void Term::collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const {
    class CollectingVisitor : public ConstVisitor {
    public:
        CollectingVisitor(std::vector<FunctionSymbolIndex> &vector)
            : vector(vector) {
        }

        void visitPre(const FunctionSymbol &funSymbol) override {
            vector.push_back(funSymbol.getFunctionSymbol());
        }

    private:
        std::vector<FunctionSymbolIndex> &vector;
    };

    CollectingVisitor visitor(vector);
    traverse(visitor);
}


std::set<FunctionSymbolIndex> Term::getFunctionSymbols() const {
    std::set<FunctionSymbolIndex> set;
    collectFunctionSymbols(set);
    return set;
}


std::vector<FunctionSymbolIndex> Term::getFunctionSymbolsAsVector() const {
    std::vector<FunctionSymbolIndex> vector;
    collectFunctionSymbols(vector);
    return vector;
}

bool Term::containsNoFunctionSymbols() const {
    class NoFuncSymbolsVisitor : public ConstVisitor {
    public:
        NoFuncSymbolsVisitor(bool &noFuncSymbols)
            : noFuncSymbols(noFuncSymbols) {
        }

        void visitPre(const FunctionSymbol &funcSym) override {
            noFuncSymbols = false;
        }

    private:
        bool &noFuncSymbols;
    };

    bool noFuncSymbols = true;
    NoFuncSymbolsVisitor visitor(noFuncSymbols);
    traverse(visitor);

    return noFuncSymbols;
}


GiNaC::ex Term::toGiNaC() const {
    throw UnsupportedOperationException();
}


std::ostream& operator<<(std::ostream &os, const Term &term) {
    class PrintVisitor : public Term::ConstVisitor {
    public:
        PrintVisitor(std::ostream &os)
            : os(os) {
        }
        void visitPre(const Addition &add) override {
            os << "(";
        }
        void visitIn(const Addition &add) override {
            os << " + ";
        }
        void visitPost(const Addition &add) override {
            os << ")";
        }
        void visitPre(const Subtraction &sub) override {
            os << "(";
        }
        void visitIn(const Subtraction &sub) override {
            os << " - ";
        }
        void visitPost(const Subtraction &sub) override {
            os << ")";
        }
        void visitPre(const Multiplication &mul) override {
            os << "(";
        }
        void visitIn(const Multiplication &mul) override {
            os << " * ";
        }
        void visitPost(const Multiplication &mul) override {
            os << ")";
        }
        void visitPre(const FunctionSymbol &funcSymbol) override {
            const ITRSProblem &itrs = funcSymbol.getITRSProblem();
            FunctionSymbolIndex funcIndex = funcSymbol.getFunctionSymbol();

            os << itrs.getFunctionSymbol(funcIndex).getName() << "(";
        }
        void visitIn(const FunctionSymbol &funcSymbol) override {
            os << ", ";
        }
        void visitPost(const FunctionSymbol &funcSymbol) override {
            os << ")";
        }
        void visit(const GiNaCExpression &expr) override {
            os << expr.getExpression();
        }

    private:
        std::ostream &os;
    };

    PrintVisitor printVisitor(os);
    term.traverse(printVisitor);

    return os;
}


Addition::Addition(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : Term(itrs),
      l(l), r(r) {
}


void Addition::traverse(Visitor &visitor) {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


void Addition::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


std::shared_ptr<Term> Addition::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression &addToCost,
                                                 GuardList &addToGuard) {
    return std::make_shared<Addition>(getITRSProblem(),
                                      l->evaluateFunction(funDef, addToCost, addToGuard),
                                      r->evaluateFunction(funDef, addToCost, addToGuard));
}

GiNaC::ex Addition::toGiNaC() const {
    return l->toGiNaC() + r->toGiNaC();
}


Purrs::Expr Addition::toPurrs() const {
    return l->toPurrs() + r->toPurrs();
}


std::shared_ptr<Term> Addition::ginacify() const {
    if (containsNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(getITRSProblem(), toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Addition>(getITRSProblem(), newL, newR);
    }
}


Subtraction::Subtraction(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : Term(itrs),
      l(l), r(r) {
}


void Subtraction::traverse(Visitor &visitor) {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


void Subtraction::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


std::shared_ptr<Term> Subtraction::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression &addToCost,
                                                 GuardList &addToGuard) {
    return std::make_shared<Subtraction>(getITRSProblem(),
                                         l->evaluateFunction(funDef, addToCost, addToGuard),
                                         r->evaluateFunction(funDef, addToCost, addToGuard));
}


GiNaC::ex Subtraction::toGiNaC() const {
    return l->toGiNaC() - r->toGiNaC();
}


Purrs::Expr Subtraction::toPurrs() const {
    return l->toPurrs() - r->toPurrs();
}


std::shared_ptr<Term> Subtraction::ginacify() const {
    if (containsNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(getITRSProblem(), toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Subtraction>(getITRSProblem(), newL, newR);
    }
}


Multiplication::Multiplication(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : Term(itrs),
      l(l), r(r) {
}


void Multiplication::traverse(Visitor &visitor) {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


void Multiplication::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


std::shared_ptr<Term> Multiplication::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression &addToCost,
                                                 GuardList &addToGuard) {
    return std::make_shared<Multiplication>(getITRSProblem(),
                                            l->evaluateFunction(funDef, addToCost, addToGuard),
                                            r->evaluateFunction(funDef, addToCost, addToGuard));
}


GiNaC::ex Multiplication::toGiNaC() const {
    return l->toGiNaC() * r->toGiNaC();
}


Purrs::Expr Multiplication::toPurrs() const {
    return l->toPurrs() * r->toPurrs();
}


std::shared_ptr<Term> Multiplication::ginacify() const {
    if (containsNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(getITRSProblem(), toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Multiplication>(getITRSProblem(), newL, newR);
    }
}


FunctionSymbol::FunctionSymbol(const ITRSProblem &itrs, FunctionSymbolIndex functionSymbol, const std::vector<std::shared_ptr<Term>> &args)
    : Term(itrs),
      functionSymbol(functionSymbol), args(args) {
}


void FunctionSymbol::traverse(Visitor &visitor) {
    visitor.visitPre(*this);

    for (int i = 0; i + 1 < args.size(); ++i) {
        args[i]->traverse(visitor);
        visitor.visitIn(*this);
    }

    if (args.size() > 0) {
        args.back()->traverse(visitor);
    }
    visitor.visitPost(*this);
}


void FunctionSymbol::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    for (int i = 0; i + 1 < args.size(); ++i) {
        args[i]->traverse(visitor);
        visitor.visitIn(*this);
    }

    if (args.size() > 0) {
        args.back()->traverse(visitor);
    }
    visitor.visitPost(*this);
}

std::shared_ptr<Term> FunctionSymbol::evaluateFunction(const FunctionDefinition &funDef,
                                                       Expression &addToCost,
                                                       GuardList &addToGuard) {

    // evaluate arguments first
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->evaluateFunction(funDef, addToCost, addToGuard));
    }

    if (funDef.getFunctionSymbol().getName()
        == getITRSProblem().getFunctionSymbol(functionSymbol).getName()) {
        const std::vector<VariableIndex> &vars = funDef.getFunctionSymbol().getArguments();
        assert(vars.size() == args.size());

        // TODO
        // build the substitution: variable -> passed argument
        GiNaC::exmap sub;
        for (int i = 0; i < vars.size(); ++i) {
            sub.emplace(getITRSProblem().getGinacSymbol(vars[i]), newArgs[i]->toGiNaC());
        }

        // apply the sub
        addToCost += funDef.getCost().subs(sub);

        for (const Expression &ex : funDef.getGuard()) {
            addToGuard.push_back(ex.subs(sub));
        }

        return std::make_shared<GiNaCExpression>(getITRSProblem(), funDef.getDefinition().subs(sub));

    } else {
        return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, newArgs);
    }
}


FunctionSymbolIndex FunctionSymbol::getFunctionSymbol() const {
    return functionSymbol;
}


Purrs::Expr FunctionSymbol::toPurrs() const {
    assert(args.size() == 1);
    return Purrs::x(args[0]->toPurrs());
}


std::shared_ptr<Term> FunctionSymbol::ginacify() const {
    std::vector<std::shared_ptr<Term>> newArgs;

    for (const std::shared_ptr<Term> &term : args) {
        if (term->containsNoFunctionSymbols()) {
            newArgs.push_back(std::make_shared<GiNaCExpression>(getITRSProblem(), term->toGiNaC()));

        } else {
            newArgs.push_back(term->ginacify());
        }
    }

    return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, newArgs);
}


GiNaCExpression::GiNaCExpression(const ITRSProblem &itrs, const GiNaC::ex &expr)
    : Term(itrs),
      expression(expr) {
}


void GiNaCExpression::traverse(Visitor &visitor) {
    visitor.visit(*this);
}


void GiNaCExpression::traverse(ConstVisitor &visitor) const {
    visitor.visit(*this);
}


std::shared_ptr<Term> GiNaCExpression::evaluateFunction(const FunctionDefinition &funDef,
                                                       Expression &addToCost,
                                                       GuardList &addToGuard) {

    return std::make_shared<GiNaCExpression>(getITRSProblem(), expression);
}


GiNaC::ex GiNaCExpression::getExpression() const {
    return expression;
}


void GiNaCExpression::setExpression(const GiNaC::ex &expr) {
    expression = expr;
}


GiNaC::ex GiNaCExpression::toGiNaC() const {
    return expression;
}


Purrs::Expr GiNaCExpression::toPurrs() const {
    return Purrs::Expr::fromGiNaC(expression);
}


std::shared_ptr<Term> GiNaCExpression::ginacify() const {
    return std::make_shared<GiNaCExpression>(getITRSProblem(), expression);
}

};