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

    return *this;
}


Expression::Expression(Expression &&ex) {
    root = ex.root;
    ex.root = nullptr;
}


Expression& Expression::operator=(Expression &&r) {
    root = r.root;
    r.root = nullptr;

    return *this;
}


Expression Expression::operator+(const GiNaC::ex &rhs) const {
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Addition>(root->getITRSProblem(), root, r));
}


Expression Expression::operator-(const GiNaC::ex &rhs) const {
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Subtraction>(root->getITRSProblem(), root, r));
}


Expression Expression::operator*(const GiNaC::ex &rhs) const {
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Multiplication>(root->getITRSProblem(), root, r));
}


Expression Expression::operator==(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::EQUAL, root, r));
}


Expression Expression::operator!=(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::NOT_EQUAL, root, r));
}


Expression Expression::operator<(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::LESS, root, r));
}


Expression Expression::operator<=(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::LESS_EQUAL, root, r));
}


Expression Expression::operator>(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::GREATER, root, r));
}


Expression Expression::operator>=(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(root->getITRSProblem(), rhs);
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::GREATER_EQUAL, root, r));
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


Expression Expression::operator==(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::EQUAL, root, rhs.root));
}


Expression Expression::operator!=(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::NOT_EQUAL, root, rhs.root));
}


Expression Expression::operator<(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::LESS, root, rhs.root));
}


Expression Expression::operator<=(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::LESS_EQUAL, root, rhs.root));
}


Expression Expression::operator>(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::GREATER, root, rhs.root));
}


Expression Expression::operator>=(const Expression &rhs) const {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(root->getITRSProblem(), Relation::GREATER_EQUAL, root, rhs.root));
}


Expression& Expression::operator+=(const Expression &rhs) {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    root = std::make_shared<Addition>(root->getITRSProblem(), root, rhs.root);
    return *this;
}


Expression& Expression::operator-=(const Expression &rhs) {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    root = std::make_shared<Subtraction>(root->getITRSProblem(), root, rhs.root);
    return *this;

}


Expression& Expression::operator*=(const Expression &rhs) {
    assert(&root->getITRSProblem() == &rhs.root->getITRSProblem());
    root = std::make_shared<Multiplication>(root->getITRSProblem(), root, rhs.root);
    return *this;

}


int Expression::nops() const {
    return root->nops();
}


Expression Expression::op(int i) const {
    return Expression(root->op(i));
}


bool Expression::info(InfoFlag info) const {
    return root->info(info);
}


bool Expression::has(const ExprSymbol &sym) const {
    return root->has(sym);
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


void Expression::collectUpdates(std::vector<Expression> &updates) const {
    return root->collectUpdates(updates);
}


std::vector<Expression> Expression::getUpdates() const {
    return root->getUpdates();
}


void Expression::collectFunctionApplications(std::vector<Expression> &app) const {
    return root->collectFunctionApplications(app);
}


std::vector<Expression> Expression::getFunctionApplications() const {
    return root->getFunctionApplications();
}


bool Expression::containsNoFunctionSymbols() const {
    return root->containsNoFunctionSymbols();
}


bool Expression::containsExactlyOneFunctionSymbol() const {
    return root->containsExactlyOneFunctionSymbol();
}


Expression Expression::substitute(const Substitution &sub) const {
    return Expression(root->substitute(sub));
}


Expression Expression::substitute(const GiNaC::exmap &sub) const {
    return Expression(root->substitute(sub));
}


Expression Expression::evaluateFunction(const FunctionDefinition &funDef,
                                        Expression *addToCost,
                                        ExpressionVector *addToGuard) const {
    return Expression(root->evaluateFunction(funDef, addToCost, addToGuard));
}


GiNaC::ex Expression::toGiNaC(bool subFunSyms) const {
    return root->toGiNaC(subFunSyms);
}


Purrs::Expr Expression::toPurrs(int i) const {
    return root->toPurrs(i);
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


FunctionDefinition::FunctionDefinition(const FunctionSymbolIndex fs,
                                       const Expression &def,
                                       const Expression &cost,
                                       const ExpressionVector &guard)
    : functionSymbol(fs), definition(def.unGinacify()), cost(cost.unGinacify()) {
    for (const Expression &ex : guard) {
        this->guard.push_back(ex.unGinacify());
    }
}


const FunctionSymbolIndex FunctionDefinition::getFunctionSymbol() const {
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

    } else if (GiNaC::is_a<GiNaC::power>(ex)) {
        res = std::make_shared<Power>(itrs, fromGiNaC(itrs, ex.op(0)), fromGiNaC(itrs, ex.op(1)));

    } else if (GiNaC::is_a<GiNaC::numeric>(ex)
               || GiNaC::is_a<GiNaC::symbol>(ex)) {
        res = std::make_shared<GiNaCExpression>(itrs, ex);

    } else if (GiNaC::is_a<GiNaC::relational>(ex)) {
        Relation::Type type;
        if (ex.info(GiNaC::info_flags::relation_equal)) {
            type = Relation::EQUAL;

        } else if (ex.info(GiNaC::info_flags::relation_not_equal)) {
            type = Relation::NOT_EQUAL;

        } else if (ex.info(GiNaC::info_flags::relation_greater)) {
            type = Relation::GREATER;

        } else if (ex.info(GiNaC::info_flags::relation_greater_or_equal)) {
            type = Relation::GREATER_EQUAL;

        } else if (ex.info(GiNaC::info_flags::relation_less)) {
            type = Relation::LESS;

        } else if (ex.info(GiNaC::info_flags::relation_less_or_equal)) {
            type = Relation::LESS_EQUAL;

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


bool Term::has(const ExprSymbol &sym) const {
    class HasVisitor : public Term::ConstVisitor {
    public:
        HasVisitor(const ExprSymbol &symbol)
            : symbol(symbol), hasSym(false) {
        }

        void visit(const GiNaCExpression &expr) override {
            hasSym = hasSym || expr.has(symbol);
        }

        bool hasSymbol() const {
            return hasSym;
        }

    private:
        const ExprSymbol &symbol;
        bool hasSym;
    };

    HasVisitor visitor(sym);
    traverse(visitor);
    return visitor.hasSymbol();
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


void Term::collectUpdates(std::vector<Expression> &updates) const {
    class CollectingVisitor : public Term::ConstVisitor {
    public:
        CollectingVisitor(std::vector<Expression> &updates)
            : updates(updates) {
        }

        void visitPre(const FunctionSymbol &funSymbol) override {
            for (const std::shared_ptr<Term> &arg : funSymbol.getArguments()) {
                updates.push_back(Expression(arg));
            }
        }

    private:
        std::vector<Expression> &updates;
    };

    CollectingVisitor visitor(updates);
    traverse(visitor);
}


std::vector<Expression> Term::getUpdates() const {
    std::vector<Expression> updates;
    collectUpdates(updates);
    return updates;
}


void Term::collectFunctionApplications(std::vector<Expression> &app) const {
    class CollectingVisitor : public Term::ConstVisitor {
    public:
        CollectingVisitor(std::vector<Expression> &app)
            : app(app) {
        }

        void visitPre(const FunctionSymbol &funSymbol) override {
            app.push_back(Expression(funSymbol.copy()));
        }

    private:
        std::vector<Expression> &app;
    };

    CollectingVisitor visitor(app);
    traverse(visitor);
}


std::vector<Expression> Term::getFunctionApplications() const {
    std::vector<Expression> app;
    collectFunctionApplications(app);
    return app;
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


bool Term::containsExactlyOneFunctionSymbol() const {
    class ExactlyOneFunSymbolVisitor : public Term::ConstVisitor {
    public:
        ExactlyOneFunSymbolVisitor(bool &oneFunSymbol)
            : oneFunSymbol(oneFunSymbol) {
            funSymbol = false;
            oneFunSymbol = 0;
        }

        void visitPre(const FunctionSymbol &fs) override {
            if (oneFunSymbol) {
                if (funSymbol != fs.getFunctionSymbol()) {
                    oneFunSymbol = false;
                    funSymbol = -1;
                }

            } else if (oneFunSymbol != -1) {
                oneFunSymbol = true;
                funSymbol = fs.getFunctionSymbol();
            }
        }

    private:
        bool &oneFunSymbol;
        FunctionSymbolIndex funSymbol;
    };

    bool oneFunSymbol;
    ExactlyOneFunSymbolVisitor visitor(oneFunSymbol);
    traverse(visitor);

    return oneFunSymbol;
}


GiNaC::ex Term::toGiNaC(bool subFunSyms) const {
    throw UnsupportedOperationException();
}


Purrs::Expr Term::toPurrs(int i) const {
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
        void visitPre(const Power &pow) override {
            os << "(";
        }
        void visitIn(const Power &pow) override {
            os << " ^ ";
        }
        void visitPost(const Power &pow) override {
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
            os << "(" << expr.getExpression() << ")";
        }

    private:
        std::ostream &os;
    };

    PrintVisitor printVisitor(os);
    term.traverse(printVisitor);

    return os;
}

const char* Relation::TypeNames[] = { "==", "!=", ">", ">=", "<", "<=" };

Relation::Relation(const ITRSProblem &itrs, Type type, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : Term(itrs), type(type), l(l), r(r) {
}


void Relation::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


int Relation::nops() const {
    return 2;
}


std::shared_ptr<Term> Relation::op(int i) const {
    assert(i >= 0 && i < nops());

    if (i == 0) {
        return l;

    } else {
        return r;
    }
}


bool Relation::info(InfoFlag info) const {
    return info == InfoFlag::Relation || info == getTypeInfoFlag();
}


std::shared_ptr<Term> Relation::copy() const {
    return std::make_shared<Relation>(getITRSProblem(), type, l, r);
}


std::shared_ptr<Term> Relation::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
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


InfoFlag Relation::getTypeInfoFlag() const {
    switch (type) {
        case EQUAL:
            return InfoFlag::RelationEqual;
            break;

        case NOT_EQUAL:
            return InfoFlag::RelationNotEqual;
            break;

        case GREATER:
            return InfoFlag::RelationGreater;
            break;

        case GREATER_EQUAL:
            return InfoFlag::RelationGreaterEqual;
            break;

        case LESS:
            return InfoFlag::RelationLess;
            break;

        case LESS_EQUAL:
            return InfoFlag::RelationLessEqual;
            break;

        default:
            assert(false);
    }
}


GiNaC::ex Relation::toGiNaC(bool subFunSyms) const {
    GiNaC::ex newL = l->toGiNaC(subFunSyms);
    GiNaC::ex newR = r->toGiNaC(subFunSyms);

    if (type == EQUAL) {
        return newL == newR;

    } else if (type == NOT_EQUAL) {
        return newL != newR;

    } else if (type == GREATER) {
        return newL > newR;

    } else if (type == GREATER_EQUAL) {
        return newL >= newR;

    } else if (type == LESS) {
        return newL < newR;

    } else if (type == LESS_EQUAL) {
        return newL <= newR;

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


void Addition::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


int Addition::nops() const {
    return 2;
}


std::shared_ptr<Term> Addition::op(int i) const {
    assert(i >= 0 && i < nops());

    if (i == 0) {
        return l;

    } else {
        return r;
    }
}


bool Addition::info(InfoFlag info) const {
    return info == InfoFlag::Addition;
}


std::shared_ptr<Term> Addition::copy() const {
    return std::make_shared<Addition>(getITRSProblem(), l, r);
}


std::shared_ptr<Term> Addition::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
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


GiNaC::ex Addition::toGiNaC(bool subFunSyms) const {
    return l->toGiNaC(subFunSyms) + r->toGiNaC(subFunSyms);
}


Purrs::Expr Addition::toPurrs(int i) const {
    return l->toPurrs(i) + r->toPurrs(i);
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


void Subtraction::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


int Subtraction::nops() const {
    return 2;
}


std::shared_ptr<Term> Subtraction::op(int i) const {
    assert(i >= 0 && i < nops());

    if (i == 0) {
        return l;

    } else {
        return r;
    }
}


bool Subtraction::info(InfoFlag info) const {
    return info == InfoFlag::Subtraction;
}


std::shared_ptr<Term> Subtraction::copy() const {
    return std::make_shared<Subtraction>(getITRSProblem(), l, r);
}


std::shared_ptr<Term> Subtraction::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
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


GiNaC::ex Subtraction::toGiNaC(bool subFunSyms) const {
    return l->toGiNaC(subFunSyms) - r->toGiNaC(subFunSyms);
}


Purrs::Expr Subtraction::toPurrs(int i) const {
    return l->toPurrs(i) - r->toPurrs(i);
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


void Multiplication::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


int Multiplication::nops() const {
    return 2;
}


std::shared_ptr<Term> Multiplication::op(int i) const {
    assert(i >= 0 && i < nops());

    if (i == 0) {
        return l;

    } else {
        return r;
    }
}


bool Multiplication::info(InfoFlag info) const {
    return info == InfoFlag::Multiplication;
}


std::shared_ptr<Term> Multiplication::copy() const {
    return std::make_shared<Multiplication>(getITRSProblem(), l, r);
}


std::shared_ptr<Term> Multiplication::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
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


GiNaC::ex Multiplication::toGiNaC(bool subFunSyms) const {
    return l->toGiNaC(subFunSyms) * r->toGiNaC(subFunSyms);
}


std::shared_ptr<Term> Multiplication::unGinacify() const {
    return std::make_shared<Multiplication>(getITRSProblem(), l->unGinacify(), r->unGinacify());
}


Purrs::Expr Multiplication::toPurrs(int i) const {
    return l->toPurrs(i) * r->toPurrs(i);
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


Power::Power(const ITRSProblem &itrs, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : Term(itrs), l(l), r(r) {
}


void Power::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    l->traverse(visitor);

    visitor.visitIn(*this);

    r->traverse(visitor);

    visitor.visitPost(*this);
}


int Power::nops() const {
    return 2;
}


std::shared_ptr<Term> Power::op(int i) const {
    assert(i >= 0 && i < nops());

    if (i == 0) {
        return l;

    } else {
        return r;
    }
}


bool Power::info(InfoFlag info) const {
    return info == InfoFlag::Power;
}


std::shared_ptr<Term> Power::copy() const {
    return std::make_shared<Power>(getITRSProblem(), l, r);
}


std::shared_ptr<Term> Power::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
    return std::make_shared<Power>(getITRSProblem(),
                                            l->evaluateFunction(funDef, addToCost, addToGuard),
                                            r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Power::substitute(const Substitution &sub) const {
    return std::make_shared<Power>(getITRSProblem(),
                                            l->substitute(sub),
                                            r->substitute(sub));
}


std::shared_ptr<Term> Power::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Power>(getITRSProblem(),
                                            l->substitute(sub),
                                            r->substitute(sub));
}


GiNaC::ex Power::toGiNaC(bool subFunSyms) const {
    return GiNaC::pow(l->toGiNaC(subFunSyms), r->toGiNaC(subFunSyms));
}


std::shared_ptr<Term> Power::unGinacify() const {
    return std::make_shared<Power>(getITRSProblem(), l->unGinacify(), r->unGinacify());
}


Purrs::Expr Power::toPurrs(int i) const {
    return Purrs::pwr(l->toPurrs(i), r->toPurrs(i));
}


std::shared_ptr<Term> Power::ginacify() const {
    if (containsNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(getITRSProblem(), toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Power>(getITRSProblem(), newL, newR);
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


int FunctionSymbol::nops() const {
    return args.size();
}


std::shared_ptr<Term> FunctionSymbol::op(int i) const {
    assert(i >= 0 && i < nops());

    return args[i];
}


bool FunctionSymbol::info(InfoFlag info) const {
    return info == InfoFlag::FunctionSymbol;
}


std::shared_ptr<Term> FunctionSymbol::copy() const {
    return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, args);
}


std::shared_ptr<Term> FunctionSymbol::evaluateFunction(const FunctionDefinition &funDef,
                                                       Expression *addToCost,
                                                       ExpressionVector *addToGuard) const {
    debugTerm("evaluate: at " << *this);
    // evaluate arguments first
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->evaluateFunction(funDef, addToCost, addToGuard));
    }

    std::string funSymName = getITRSProblem().getFunctionSymbol(functionSymbol).getName();
    const ::FunctionSymbol &funSymbol = getITRSProblem().getFunctionSymbol(funDef.getFunctionSymbol());
    if (funSymbol.getName() == funSymName) {
        const std::vector<VariableIndex> &vars = funSymbol.getArguments();
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
        if (addToCost) {
            *addToCost = *addToCost + funDef.getCost().substitute(sub);
        }

        if (addToGuard) {
            for (const Expression &ex : funDef.getGuard()) {
                addToGuard->push_back(ex.substitute(sub));
            }
        }

        debugTerm("funDef: " << funDef.getDefinition());
        std::shared_ptr<Term> result = funDef.getDefinition().getTermTree()->substitute(sub);
        debugTerm("result: " << *result);

        return result;

    } else {
        return std::make_shared<FunctionSymbol>(getITRSProblem(), functionSymbol, newArgs);
    }
}


FunctionSymbolIndex FunctionSymbol::getFunctionSymbol() const {
    return functionSymbol;
}


const std::vector<std::shared_ptr<Term>>& FunctionSymbol::getArguments() const {
    return args;
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


GiNaC::ex FunctionSymbol::toGiNaC(bool subFunSyms) const {
    if (subFunSyms) {
        return ExprSymbol();

    } else {
        throw UnsupportedOperationException();
    }
}


Purrs::Expr FunctionSymbol::toPurrs(int i) const {
    assert(i >= 0);
    assert(i < args.size());
    return Purrs::x(args[i]->toPurrs(i));
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


void GiNaCExpression::traverse(ConstVisitor &visitor) const {
    visitor.visit(*this);
}


int GiNaCExpression::nops() const {
    return expression.nops();
}


std::shared_ptr<Term> GiNaCExpression::op(int i) const {
    assert(i >= 0 && i < expression.nops());

    return std::make_shared<GiNaCExpression>(getITRSProblem(), expression.op(i));
}


bool GiNaCExpression::info(InfoFlag info) const {
    switch (info) {
        case InfoFlag::Relation:
            return expression.info(GiNaC::info_flags::relation);

        case InfoFlag::RelationEqual:
            return expression.info(GiNaC::info_flags::relation_equal);

        case InfoFlag::RelationNotEqual:
            return expression.info(GiNaC::info_flags::relation_not_equal);

        case InfoFlag::RelationGreater:
            return expression.info(GiNaC::info_flags::relation_greater);

        case InfoFlag::RelationGreaterEqual:
            return expression.info(GiNaC::info_flags::relation_greater_or_equal);

        case InfoFlag::RelationLess:
            return expression.info(GiNaC::info_flags::relation_less);

        case InfoFlag::RelationLessEqual:
            return expression.info(GiNaC::info_flags::relation_less_or_equal);

        case InfoFlag::Addition:
            return GiNaC::is_a<GiNaC::add>(expression);

        case InfoFlag::Multiplication:
            return GiNaC::is_a<GiNaC::mul>(expression);

        case InfoFlag::Power:
            return GiNaC::is_a<GiNaC::power>(expression);

        case InfoFlag::Number:
            return GiNaC::is_a<GiNaC::numeric>(expression);

        default:
            return false;
    }
}


std::shared_ptr<Term> GiNaCExpression::copy() const {
    return std::make_shared<GiNaCExpression>(getITRSProblem(), expression);
}


std::shared_ptr<Term> GiNaCExpression::evaluateFunction(const FunctionDefinition &funDef,
                                                        Expression *addToCost,
                                                        ExpressionVector *addToGuard) const {
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


GiNaC::ex GiNaCExpression::toGiNaC(bool subFunSyms) const {
    return expression;
}


Purrs::Expr GiNaCExpression::toPurrs(int i) const {
    return Purrs::Expr::fromGiNaC(expression);
}


std::shared_ptr<Term> GiNaCExpression::ginacify() const {
    return std::make_shared<GiNaCExpression>(getITRSProblem(), expression);
}


std::shared_ptr<Term> GiNaCExpression::unGinacify() const {
    return fromGiNaC(getITRSProblem(), expression);
}

};