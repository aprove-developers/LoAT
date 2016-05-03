#include "term.h"

#include "itrs.h"

#include "expression.h"

namespace TT {

Expression::Expression()
    : root(nullptr) {
}


Expression::Expression(const ITRSProblem &itrs, const GiNaC::ex &ex) {
    root = std::make_shared<GiNaCExpression>(itrs, ex);
}


Expression::Expression(const ITRSProblem &itrs, const FunctionSymbolIndex functionSymbol,
           const std::vector<Expression> &args) {
    root = std::make_shared<FunctionSymbol>(itrs, functionSymbol, args);
}


Expression::Expression(const Expression &ex) {
    if (ex.root != nullptr) {
        root = ex.root->copy();

    } else {
        root = nullptr;
    }
}


Expression& Expression::operator=(const Expression &r) {
    if (r.root != nullptr) {
        root = r.root->copy();

    } else {
        root = nullptr;
    }
}


Expression Expression::operator+(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    return Expression(std::make_shared<Addition>(root->getITRSProblem(), root, rhs.root));
}


Expression Expression::operator-(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    return Expression(std::make_shared<Subtraction>(root->getITRSProblem(), root, rhs.root));
}


Expression Expression::operator*(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    return Expression(std::make_shared<Multiplication>(root->getITRSProblem(), root, rhs.root));
}


void Expression::collectVariables(ExprSymbolSet &set) const {
    root->collectVariables(set);
}


ExprSymbolSet Expression::getVariables() const {
    return root->getVariables();
}


void Expression::collectFunctionSymbols(std::set<FunctionSymbolIndex> &set) const {
    root->collectFunctionSymbols(set);
}


std::set<FunctionSymbolIndex> Expression::getFunctionSymbols() const {
    return root->getFunctionSymbols();
}


void Expression::collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const {
    root->collectFunctionSymbols(vector);
}


std::vector<FunctionSymbolIndex> Expression::getFunctionSymbolsAsVector() const {
    return root->getFunctionSymbolsAsVector();
}


bool Expression::containsNoFunctionSymbols() const {
    return root->containsNoFunctionSymbols();
}


Expression Expression::substitute(const Substitution &sub) const {
    return Expression(root->substitute(sub));
}


Expression Expression::substitute(const GiNaC::exmap &sub) const {
    return Expression(root->substitute(sub));
}


Expression Expression::evaluateFunction(const FunctionDefinition &funDef,
                                        Expression &addToCost,
                                        ExpressionVector &addToGuard) const {
    return Expression(root->evaluateFunction(funDef, addToCost, addToGuard));
}


GiNaC::ex Expression::toGiNaC() const {
    return root->toGiNaC();
}


Purrs::Expr Expression::toPurrs() const {
    return root->toPurrs();
}


Expression Expression::ginacify() const {
    return Expression(root->ginacify());
}


Expression Expression::unGinacify() const {
    return Expression(root->unGinacify());
}


Expression::Expression(std::shared_ptr<Term> root)
    : root(root) {
}


std::shared_ptr<Term> Expression::getTermTree() const {
    return root;
}


std::ostream& operator<<(std::ostream &os, const Expression &ex) {
    os << *ex.getTermTree();
    return os;
}


FunctionDefinition::FunctionDefinition(const ::FunctionSymbol &fs,
                                       const Expression &def,
                                       const Expression &cost,
                                       const ExpressionVector &guard)
    : functionSymbol(fs), definition(def.unGinacify()), cost(cost.unGinacify()) {
    for (const Expression &ex : guard) {
        this->guard.push_back(ex.unGinacify());
    }
}


const ::FunctionSymbol& FunctionDefinition::getFunctionSymbol() const {
    return functionSymbol;
}


const Expression& FunctionDefinition::getDefinition() const {
    return definition;
}


const Expression& FunctionDefinition::getCost() const {
    return cost;
}


const ExpressionVector& FunctionDefinition::getGuard() const {
    return guard;
}


std::shared_ptr<Term> Term::fromGiNaC(const ITRSProblem &itrs, const GiNaC::ex &ex) {
    std::shared_ptr<Term> res;

    if (GiNaC::is_a<GiNaC::add>(ex)) {
        res = fromGiNaC(itrs, ex.op(0));

        for (int i = 1; i < ex.nops(); ++i) {
            res = std::make_shared<Addition>(itrs, res, fromGiNaC(itrs, ex.op(i)));
        }

    } else if (GiNaC::is_a<GiNaC::mul>(ex)) {
        res = fromGiNaC(itrs, ex.op(0));

        for (int i = 1; i < ex.nops(); ++i) {
            res = std::make_shared<Multiplication>(itrs, res, fromGiNaC(itrs, ex.op(i)));
        }

    } else if (GiNaC::is_a<GiNaC::numeric>(ex)
               || GiNaC::is_a<GiNaC::symbol>(ex)) {
        res = std::make_shared<GiNaCExpression>(itrs, ex);

    } else if (GiNaC::is_a<GiNaC::relational>(ex)) {
        Relation::Type type;
        if (ex.info(GiNaC::info_flags::relation_equal)) {
            type = Relation::EQUAL;

        } else if (ex.info(GiNaC::info_flags::relation_greater_or_equal)) {
            type = Relation::GREATER_EQUAL;

        } else if (ex.info(GiNaC::info_flags::relation_greater)) {
            type = Relation::GREATER;

        } else {
            debugTerm(ex);
            throw UnsupportedExpressionException();
        }

        res = std::make_shared<Relation>(itrs, type,
                                         fromGiNaC(itrs, ex.op(0)), fromGiNaC(itrs, ex.op(1)));

    } else {
        throw UnsupportedExpressionException();
    }

    return res;
}

Term::Term(const ITRSProblem &itrs)
    : itrs(itrs) {
}


Term::~Term() {
}


const ITRSProblem& Term::getITRSProblem() const {
    return itrs;
}


void Term::collectVariables(ExprSymbolSet &set) const {
    class CollectingVisitor : public Term::ConstVisitor {
    public:
        CollectingVisitor(ExprSymbolSet &set)
            : set(set) {
        }

        void visit(const GiNaCExpression &expr) override {
            ::Expression customExpr(expr.getExpression());
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
    class CollectingVisitor : public Term::ConstVisitor {
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


std::set<FunctionSymbolIndex> Term::getFunctionSymbols() const {
    std::set<FunctionSymbolIndex> set;
    collectFunctionSymbols(set);
    return set;
}


void Term::collectFunctionSymbols(std::vector<FunctionSymbolIndex> &vector) const {
    class CollectingVisitor : public Term::ConstVisitor {
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


std::vector<FunctionSymbolIndex> Term::getFunctionSymbolsAsVector() const {
    std::vector<FunctionSymbolIndex> vector;
    collectFunctionSymbols(vector);
    return vector;
}


bool Term::containsNoFunctionSymbols() const {
    class NoFuncSymbolsVisitor : public Term::ConstVisitor {
    public:
        NoFuncSymbolsVisitor(bool &noFuncSymbols)
            : noFuncSymbols(noFuncSymbols) {
            noFuncSymbols = true;
        }

        void visitPre(const FunctionSymbol &funcSym) override {
            noFuncSymbols = false;
        }

    private:
        bool &noFuncSymbols;
    };

    bool noFuncSymbols;
    NoFuncSymbolsVisitor visitor(noFuncSymbols);
    traverse(visitor);

    return noFuncSymbols;
}


GiNaC::ex Term::toGiNaC() const {
    throw UnsupportedOperationException();
}


Purrs::Expr Term::toPurrs() const {
    throw UnsupportedOperationException();
}


std::ostream& operator<<(std::ostream &os, const Term &term) {
    class PrintVisitor : public Term::ConstVisitor {
    public:
        PrintVisitor(std::ostream &os)
            : os(os) {
        }
        void visitIn(const Relation &rel) override {
            os << " " << Relation::TypeNames[rel.getType()] << " ";
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

const char* Relation::TypeNames[] = { "==", ">", ">=" };

Relation::Relation(const ITRSProblem &itrs, Type type, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : Term(itrs), type(type), l(l), r(r) {
}


void Relation::traverse(Visitor &visitor) {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


void Relation::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


std::shared_ptr<Term> Relation::copy() const {
    return std::make_shared<Relation>(getITRSProblem(), type, l, r);
}


std::shared_ptr<Term> Relation::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression &addToCost,
                                                 ExpressionVector &addToGuard) const {
    return std::make_shared<Relation>(getITRSProblem(),
                                      type,
                                      l->evaluateFunction(funDef, addToCost, addToGuard),
                                      r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Relation::substitute(const Substitution &sub) const {
    return std::make_shared<Relation>(getITRSProblem(),
                                      type,
                                      l->substitute(sub),
                                      r->substitute(sub));
}


std::shared_ptr<Term> Relation::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Relation>(getITRSProblem(),
                                      type,
                                      l->substitute(sub),
                                      r->substitute(sub));
}


Relation::Type Relation::getType() const {
    return type;
}


GiNaC::ex Relation::toGiNaC() const {
    if (type == EQUAL) {
        return l->toGiNaC() == r->toGiNaC();

    } else if (type == GREATER_EQUAL) {
        return l->toGiNaC() >= r->toGiNaC();

    } else if (type == GREATER) {
        return l->toGiNaC() > r->toGiNaC();

    } else {
        throw UnsupportedExpressionException();
    }
}


std::shared_ptr<Term> Relation::ginacify() const {
    if (containsNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(getITRSProblem(), toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Relation>(getITRSProblem(), type, newL, newR);
    }
}


std::shared_ptr<Term> Relation::unGinacify() const {
    return std::make_shared<Relation>(getITRSProblem(), type, l->unGinacify(), r->unGinacify());
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


std::shared_ptr<Term> Addition::copy() const {
    return std::make_shared<Addition>(getITRSProblem(), l, r);
}


std::shared_ptr<Term> Addition::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression &addToCost,
                                                 ExpressionVector &addToGuard) const {
    return std::make_shared<Addition>(getITRSProblem(),
                                      l->evaluateFunction(funDef, addToCost, addToGuard),
                                      r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Addition::substitute(const Substitution &sub) const {
    return std::make_shared<Addition>(getITRSProblem(),
                                      l->substitute(sub),
                                      r->substitute(sub));
}


std::shared_ptr<Term> Addition::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Addition>(getITRSProblem(),
                                      l->substitute(sub),
                                      r->substitute(sub));
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


std::shared_ptr<Term> Addition::unGinacify() const {
    return std::make_shared<Addition>(getITRSProblem(), l->unGinacify(), r->unGinacify());
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


std::shared_ptr<Term> Subtraction::copy() const {
    return std::make_shared<Subtraction>(getITRSProblem(), l, r);
}


std::shared_ptr<Term> Subtraction::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression &addToCost,
                                                 ExpressionVector &addToGuard) const {
    return std::make_shared<Subtraction>(getITRSProblem(),
                                         l->evaluateFunction(funDef, addToCost, addToGuard),
                                         r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Subtraction::substitute(const Substitution &sub) const {
    return std::make_shared<Subtraction>(getITRSProblem(),
                                         l->substitute(sub),
                                         r->substitute(sub));
}


std::shared_ptr<Term> Subtraction::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Subtraction>(getITRSProblem(),
                                         l->substitute(sub),
                                         r->substitute(sub));
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


std::shared_ptr<Term> Subtraction::unGinacify() const {
    return std::make_shared<Subtraction>(getITRSProblem(), l->unGinacify(), r->unGinacify());
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


std::shared_ptr<Term> Multiplication::copy() const {
    return std::make_shared<Multiplication>(getITRSProblem(), l, r);
}


std::shared_ptr<Term> Multiplication::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression &addToCost,
                                                 ExpressionVector &addToGuard) const {
    return std::make_shared<Multiplication>(getITRSProblem(),
                                            l->evaluateFunction(funDef, addToCost, addToGuard),
                                            r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Multiplication::substitute(const Substitution &sub) const {
    return std::make_shared<Multiplication>(getITRSProblem(),
                                            l->substitute(sub),
                                            r->substitute(sub));
}


std::shared_ptr<Term> Multiplication::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Multiplication>(getITRSProblem(),
                                            l->substitute(sub),
                                            r->substitute(sub));
}


GiNaC::ex Multiplication::toGiNaC() const {
    return l->toGiNaC() * r->toGiNaC();
}


std::shared_ptr<Term> Multiplication::unGinacify() const {
    return std::make_shared<Multiplication>(getITRSProblem(), l->unGinacify(), r->unGinacify());
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
    : Term(itrs), functionSymbol(functionSymbol), args(args) {
}

FunctionSymbol::FunctionSymbol(const ITRSProblem &itrs, FunctionSymbolIndex functionSymbol, const std::vector<Expression> &args)
    : Term(itrs), functionSymbol(functionSymbol) {
    for (const Expression &ex : args) {
        this->args.push_back(ex.getTermTree());
    }
}


void FunctionSymbol::traverse(Visitor &visitor) {
    visitor.visitPre(*this);

    for (int i = 0; i < args.size(); ++i) {
        if (i > 0) {
            visitor.visitIn(*this);
        }

        args[i]->traverse(visitor);
    }

    visitor.visitPost(*this);
}


void FunctionSymbol::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    for (int i = 0; i < args.size(); ++i) {
        if (i > 0) {
            visitor.visitIn(*this);
        }

        args[i]->traverse(visitor);
    }

    visitor.visitPost(*this);
}


std::shared_ptr<Term> FunctionSymbol::copy() const {
    return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, args);
}


std::shared_ptr<Term> FunctionSymbol::evaluateFunction(const FunctionDefinition &funDef,
                                                       Expression &addToCost,
                                                       ExpressionVector &addToGuard) const {
    std::string funSymName = getITRSProblem().getFunctionSymbol(functionSymbol).getName();
    debugTerm("evaluate: at " << *this);
    // evaluate arguments first
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->evaluateFunction(funDef, addToCost, addToGuard));
    }

    if (funDef.getFunctionSymbol().getName() == funSymName) {
        const std::vector<VariableIndex> &vars = funDef.getFunctionSymbol().getArguments();
        assert(vars.size() == args.size());

        // build the substitution: variable -> passed argument
        debugTerm("\tbuild substitution");
        Substitution sub;
        for (int i = 0; i < vars.size(); ++i) {
            ExprSymbol var = getITRSProblem().getGinacSymbol(vars[i]);
            sub.emplace(var, Expression(newArgs[i]));
            debugTerm("\t" << var << "\\" << *newArgs[i]);
        }

        // apply the sub
        addToCost = addToCost + funDef.getCost().substitute(sub);

        for (const Expression &ex : funDef.getGuard()) {
            addToGuard.push_back(ex.substitute(sub));
        }

        return funDef.getDefinition().getTermTree()->substitute(sub);

    } else {
        return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, newArgs);
    }
}


FunctionSymbolIndex FunctionSymbol::getFunctionSymbol() const {
    return functionSymbol;
}


std::shared_ptr<Term> FunctionSymbol::substitute(const Substitution &sub) const {
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->substitute(sub));
    }

    return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, newArgs);
}


std::shared_ptr<Term> FunctionSymbol::substitute(const GiNaC::exmap &sub) const {
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->substitute(sub));
    }

    return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, newArgs);
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


std::shared_ptr<Term> FunctionSymbol::unGinacify() const {
    std::vector<std::shared_ptr<Term>> newArgs;

    for (const std::shared_ptr<Term> &term : args) {
        newArgs.push_back(term->unGinacify());
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


std::shared_ptr<Term> GiNaCExpression::copy() const {
    return std::make_shared<GiNaCExpression>(getITRSProblem(), expression);
}


std::shared_ptr<Term> GiNaCExpression::evaluateFunction(const FunctionDefinition &funDef,
                                                       Expression &addToCost,
                                                       ExpressionVector &addToGuard) const {

    return copy();
}


GiNaC::ex GiNaCExpression::getExpression() const {
    return expression;
}


void GiNaCExpression::setExpression(const GiNaC::ex &expr) {
    expression = expr;
}


std::shared_ptr<Term> GiNaCExpression::substitute(const Substitution &sub) const {
    if (GiNaC::is_a<GiNaC::symbol>(expression)) {
        Substitution::const_iterator it = sub.find(GiNaC::ex_to<GiNaC::symbol>(expression));

        if (it != sub.end()) {
            return it->second.getTermTree()->copy();
        }

    }

    return copy();
}


std::shared_ptr<Term> GiNaCExpression::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<GiNaCExpression>(getITRSProblem(), expression.subs(sub));
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


std::shared_ptr<Term> GiNaCExpression::unGinacify() const {
    return fromGiNaC(getITRSProblem(), expression);
}

};