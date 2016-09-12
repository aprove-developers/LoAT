#include "term.h"

#include <sstream>

#include "itrsproblem.h"
#include "expression.h"
#include "z3toolbox.h"

namespace TT {

Expression::Expression() {
    root = std::make_shared<GiNaCExpression>(GiNaC::numeric(0));
}


Expression::Expression(const GiNaC::ex &ex) {
    root = std::make_shared<GiNaCExpression>(ex);
}


Expression::Expression(const FunctionSymbolIndex functionSymbol,
                       const std::string &name,
                       const std::vector<Expression> &args) {
    root = std::make_shared<FunctionSymbol>(functionSymbol, name, args);
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
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Addition>(root, r));
}


Expression Expression::operator-(const GiNaC::ex &rhs) const {
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Subtraction>(root, r));
}


Expression Expression::operator*(const GiNaC::ex &rhs) const {
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Multiplication>(root, r));
}


Expression Expression::operator^(const GiNaC::ex &rhs) const {
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Power>(root, r));
}


Expression Expression::operator==(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Relation>(Relation::EQUAL, root, r));
}


Expression Expression::operator!=(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Relation>(Relation::NOT_EQUAL, root, r));
}


Expression Expression::operator<(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Relation>(Relation::LESS, root, r));
}


Expression Expression::operator<=(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Relation>(Relation::LESS_EQUAL, root, r));
}


Expression Expression::operator>(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Relation>(Relation::GREATER, root, r));
}


Expression Expression::operator>=(const GiNaC::ex &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(GiNaC::info_flags::relation));
    std::shared_ptr<Term> r = std::make_shared<GiNaCExpression>(rhs);
    return Expression(std::make_shared<Relation>(Relation::GREATER_EQUAL, root, r));
}


Expression Expression::operator+(const Expression &rhs) const {
    return Expression(std::make_shared<Addition>(root, rhs.root));
}


Expression Expression::operator-(const Expression &rhs) const {
    return Expression(std::make_shared<Subtraction>(root, rhs.root));
}


Expression Expression::operator*(const Expression &rhs) const {
    return Expression(std::make_shared<Multiplication>(root, rhs.root));
}


Expression Expression::operator^(const Expression &rhs) const {
    return Expression(std::make_shared<Power>(root, rhs.root));
}


Expression Expression::operator==(const Expression &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(Relation::EQUAL, root, rhs.root));
}


Expression Expression::operator!=(const Expression &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(Relation::NOT_EQUAL, root, rhs.root));
}


Expression Expression::operator<(const Expression &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(Relation::LESS, root, rhs.root));
}


Expression Expression::operator<=(const Expression &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(Relation::LESS_EQUAL, root, rhs.root));
}


Expression Expression::operator>(const Expression &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(Relation::GREATER, root, rhs.root));
}


Expression Expression::operator>=(const Expression &rhs) const {
    assert(!info(InfoFlag::Relation));
    assert(!rhs.info(InfoFlag::Relation));
    return Expression(std::make_shared<Relation>(Relation::GREATER_EQUAL, root, rhs.root));
}


Expression& Expression::operator+=(const Expression &rhs) {
    root = std::make_shared<Addition>(root, rhs.root);
    return *this;
}


Expression& Expression::operator-=(const Expression &rhs) {
    root = std::make_shared<Subtraction>(root, rhs.root);
    return *this;
}


Expression& Expression::operator*=(const Expression &rhs) {
    root = std::make_shared<Multiplication>(root, rhs.root);
    return *this;
}


Expression& Expression::operator^=(const Expression &rhs) {
    root = std::make_shared<Power>(root, rhs.root);
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


bool Expression::isSimple() const {
    return info(InfoFlag::FunctionSymbol) && hasExactlyOneFunctionSymbolOnce();
}


void Expression::traverse(ConstVisitor &visitor) const {
    root->traverse(visitor);
}


void Expression::collectVariables(ExprSymbolSet &set) const {
    root->collectVariables(set);
}


ExprSymbolSet Expression::getVariables() const {
    return root->getVariables();
}


void Expression::collectVariables(std::vector<ExprSymbol> &vector) const {
    root->collectVariables(vector);
}


std::vector<ExprSymbol> Expression::getVariablesAsVector() const {
    return root->getVariablesAsVector();
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


bool Expression::hasFunctionSymbol(FunctionSymbolIndex funSym) const {
    return root->hasFunctionSymbol(funSym);
}


bool Expression::hasFunctionSymbol() const {
    return root->hasFunctionSymbol();
}


bool Expression::hasNoFunctionSymbols() const {
    return root->hasNoFunctionSymbols();
}


bool Expression::hasExactlyOneFunctionSymbol() const {
    return root->hasExactlyOneFunctionSymbol();
}


bool Expression::hasExactlyOneFunctionSymbolOnce() const {
    return root->hasExactlyOneFunctionSymbolOnce();
}


Expression Expression::substitute(const Substitution &sub) const {
    return Expression(root->substitute(sub));
}


Expression Expression::substitute(const GiNaC::exmap &sub) const {
    return Expression(root->substitute(sub));
}


Expression Expression::substitute(FunctionSymbolIndex fs, const Expression &ex) const {
    return Expression(root->substitute(fs, ex.getTermTree()));
}


Expression Expression::evaluateFunction(const FunctionDefinition &funDef,
                                        Expression *addToCost,
                                        ExpressionVector *addToGuard) const {
    return Expression(root->evaluateFunction(funDef, addToCost, addToGuard));
}


Expression Expression::evaluateFunction2(const FunctionDefinition &funDef,
                                        Expression *addToCost,
                                        ExpressionVector *addToGuard) const {
    return Expression(root->evaluateFunction2(funDef, addToCost, addToGuard));
}


Expression Expression::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                               const TT::ExpressionVector &guard,
                                               Expression *addToCost,
                                               bool &modified) const {
    return Expression(root->evaluateFunctionIfLegal(funDef, guard, addToCost, modified));
}


Expression Expression::moveFunctionSymbolsToGuard(ITRSProblem &itrs, TT::ExpressionVector &guard) {
    return Expression(root->moveFunctionSymbolsToGuard(itrs, guard));
}


Expression Expression::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                    const std::map<FunctionSymbolIndex,int> &specialCases) const {
    return Expression(root->abstractSize(funSyms, specialCases));
}


GiNaC::ex Expression::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    return root->toGiNaC(subFunSyms, singleVar, sub);
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


bool Expression::equals(const Expression &other) const {
    return root->equals(other.root);
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


FunctionDefinition::FunctionDefinition(ITRSProblem &itrs,
                                       const FunctionSymbolIndex fs,
                                       const Expression &def,
                                       const Expression &cost,
                                       const ExpressionVector &guard)
    : itrs(&itrs), functionSymbol(fs), definition(def.unGinacify()), cost(cost.unGinacify()) {
    for (const Expression &ex : guard) {
        this->guard.push_back(ex.unGinacify());
    }
}


ITRSProblem& FunctionDefinition::getITRSProblem() const {
    return *itrs;
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


std::shared_ptr<Term> Term::fromGiNaC(const GiNaC::ex &ex) {
    std::shared_ptr<Term> res;

    if (GiNaC::is_a<GiNaC::add>(ex)) {
        res = fromGiNaC(ex.op(0));

        for (int i = 1; i < ex.nops(); ++i) {
            res = std::make_shared<Addition>(res, fromGiNaC(ex.op(i)));
        }

    } else if (GiNaC::is_a<GiNaC::mul>(ex)) {
        res = fromGiNaC(ex.op(0));

        for (int i = 1; i < ex.nops(); ++i) {
            res = std::make_shared<Multiplication>(res, fromGiNaC(ex.op(i)));
        }

    } else if (GiNaC::is_a<GiNaC::power>(ex)) {
        res = std::make_shared<Power>(fromGiNaC(ex.op(0)), fromGiNaC(ex.op(1)));

    } else if (GiNaC::is_a<GiNaC::numeric>(ex)
               || GiNaC::is_a<GiNaC::symbol>(ex)) {
        res = std::make_shared<GiNaCExpression>(ex);

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

        res = std::make_shared<Relation>(type, fromGiNaC(ex.op(0)), fromGiNaC(ex.op(1)));

    }  else if (GiNaC::is_a<GiNaC::function>(ex)) {
        GiNaC::ex factWildCard = GiNaC::factorial(GiNaC::wild());

        if (ex.match(factWildCard)) {
            res = std::make_shared<Factorial>(fromGiNaC(ex.op(0)));

        } else {
            throw UnsupportedExpressionException("Unknown function");
        }

    } else {
        debugTerm("FOO");
        debugTerm(ex);
        throw UnsupportedExpressionException();
    }

    return res;
}


Term::~Term() {
}


bool Term::has(const ExprSymbol &sym) const {
    class HasVisitor : public ConstVisitor {
    public:
        HasVisitor(const ExprSymbol &symbol)
            : symbol(symbol), hasSym(false) {
        }

        void visit(const GiNaCExpression &expr) override {
            hasSym = hasSym || expr.getExpression().has(symbol);
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
    class CollectingVisitor : public ConstVisitor {
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


void Term::collectVariables(std::vector<ExprSymbol> &vector) const {
    class CollectingVisitor : public ConstVisitor {
    public:
        CollectingVisitor(std::vector<ExprSymbol> &vector)
            : vector(vector) {
        }

        void visit(const GiNaCExpression &expr) override {
            ::Expression customExpr(expr.getExpression());
            customExpr.collectVariables(vector);
        }

    private:
        std::vector<ExprSymbol> &vector;
    };

    CollectingVisitor visitor(vector);
    traverse(visitor);
}


std::vector<ExprSymbol> Term::getVariablesAsVector() const {
    std::vector<ExprSymbol> vector;
    collectVariables(vector);
    return vector;
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


std::set<FunctionSymbolIndex> Term::getFunctionSymbols() const {
    std::set<FunctionSymbolIndex> set;
    collectFunctionSymbols(set);
    return set;
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


std::vector<FunctionSymbolIndex> Term::getFunctionSymbolsAsVector() const {
    std::vector<FunctionSymbolIndex> vector;
    collectFunctionSymbols(vector);
    return vector;
}


void Term::collectUpdates(std::vector<Expression> &updates) const {
    class CollectingVisitor : public ConstVisitor {
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
    class CollectingVisitor : public ConstVisitor {
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


bool Term::hasFunctionSymbol(FunctionSymbolIndex funSym) const {
    class FuncSymbolsVisitor : public ConstVisitor {
    public:
        FuncSymbolsVisitor(FunctionSymbolIndex funSymbol)
            : funSymbol(funSymbol) {
            hasThisFunSym = false;
        }

        void visitPre(const FunctionSymbol &funSym) override {
            if (funSymbol == funSym.getFunctionSymbol()) {
                hasThisFunSym = true;
            }
        }

        bool hasThisFunctionSymbol() const {
            return hasThisFunSym;
        }

    private:
        FunctionSymbolIndex funSymbol;
        bool hasThisFunSym;
    };

    FuncSymbolsVisitor visitor(funSym);
    traverse(visitor);
    return visitor.hasThisFunctionSymbol();
}

bool Term::hasFunctionSymbol() const {
    return !hasNoFunctionSymbols();
}


bool Term::hasNoFunctionSymbols() const {
    class NoFuncSymbolsVisitor : public ConstVisitor {
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


bool Term::hasExactlyOneFunctionSymbol() const {
    class ExactlyOneFunSymbolVisitor : public ConstVisitor {
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


bool Term::hasExactlyOneFunctionSymbolOnce() const {
    class ExactlyOneOnceFunSymbolVisitor : public ConstVisitor {
    public:
        ExactlyOneOnceFunSymbolVisitor(bool &oneFunSymbol)
            : oneFunSymbol(oneFunSymbol) {
            oneFunSymbol = false;
            moreThanOne = false;
        }

        void visitPre(const FunctionSymbol &fs) override {
            if (!moreThanOne) {
                if (!oneFunSymbol) {
                    oneFunSymbol = true;

                } else {
                    moreThanOne = true;
                    oneFunSymbol = false;
                }
            }
        }

    private:
        bool &oneFunSymbol;
        bool moreThanOne;
    };

    bool oneFunSymbol;
    ExactlyOneOnceFunSymbolVisitor visitor(oneFunSymbol);
    traverse(visitor);

    return oneFunSymbol;
}


std::shared_ptr<Term> Term::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                         const std::map<FunctionSymbolIndex,int> &specialCases) const {
    throw UnsupportedOperationException();
}


GiNaC::ex Term::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    throw UnsupportedOperationException();
}


Purrs::Expr Term::toPurrs(int i) const {
    throw UnsupportedOperationException();
}


std::ostream& operator<<(std::ostream &os, const Term &term) {
    class PrintVisitor : public ConstVisitor {
    public:
        PrintVisitor(std::ostream &os)
            : os(os) {
        }
        void visitPre(const Relation &rel) override {
            os << "(";
        }
        void visitIn(const Relation &rel) override {
            os << " " << Relation::TypeNames[rel.getType()] << " ";
        }
        void visitPost(const Relation &rel) override {
            os << ")";
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
        void visitPre(const Factorial &fact) override {
            os << "(";
        }
        void visitPost(const Factorial &fact) override {
            os << "!)";
        }
        void visitPre(const FunctionSymbol &funcSymbol) override {
            os << funcSymbol.getName();
            if (!funcSymbol.getArguments().empty()) {
                os << "(";
            }
        }
        void visitIn(const FunctionSymbol &funcSymbol) override {
            os << ", ";
        }
        void visitPost(const FunctionSymbol &funcSymbol) override {
            if (!funcSymbol.getArguments().empty()) {
                os << ")";
            }
        }
        void visit(const GiNaCExpression &expr) override {
            os << "(" << expr.getExpression() << ")";
        }

    private:
        std::ostream &os;
    };

    std::ostringstream oss;
    PrintVisitor printVisitor(oss);
    term.traverse(printVisitor);

    if (oss.str()[0] == '(') {
        os << oss.str().substr(1, oss.str().length() - 2);

    } else {
        os << oss.str();
    }

    return os;
}


bool Term::equals(const std::shared_ptr<Term> other) const {
    return equals(*other);
}


bool Term::equals(const Relation &term) const {
    return false;
}


bool Term::equals(const Addition &term) const {
    return false;
}


bool Term::equals(const Subtraction &term) const {
    return false;
}


bool Term::equals(const Multiplication &term) const {
    return false;
}


bool Term::equals(const Power &term) const {
    return false;
}


bool Term::equals(const Factorial &term) const {
    return false;
}


bool Term::equals(const FunctionSymbol &term) const {
    return false;
}


bool Term::equals(const GiNaCExpression &term) const {
    return false;
}


const char* Relation::TypeNames[] = { "==", "!=", ">", ">=", "<", "<=" };

Relation::Relation(Type type, const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : type(type), l(l), r(r) {
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
    return std::make_shared<Relation>(type, l, r);
}


std::shared_ptr<Term> Relation::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
    return std::make_shared<Relation>(type,
                                      l->evaluateFunction(funDef, addToCost, addToGuard),
                                      r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Relation::evaluateFunction2(const FunctionDefinition &funDef,
                                                  Expression *addToCost,
                                                  ExpressionVector *addToGuard) const {
    return std::make_shared<Relation>(type,
                                      l->evaluateFunction2(funDef, addToCost, addToGuard),
                                      r->evaluateFunction2(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Relation::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                        const TT::ExpressionVector &guard,
                                                        Expression *addToCost,
                                                        bool &modified) const {
    return std::make_shared<Relation>(type,
                                      l->evaluateFunctionIfLegal(funDef, guard, addToCost, modified),
                                      r->evaluateFunctionIfLegal(funDef, guard, addToCost, modified));
}


std::shared_ptr<Term> Relation::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                           TT::ExpressionVector &guard) const {
    return std::make_shared<Relation>(type,
                                      l->moveFunctionSymbolsToGuard(itrs, guard),
                                      r->moveFunctionSymbolsToGuard(itrs, guard));
}


std::shared_ptr<Term> Relation::substitute(const Substitution &sub) const {
    return std::make_shared<Relation>(type,
                                      l->substitute(sub),
                                      r->substitute(sub));
}


std::shared_ptr<Term> Relation::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Relation>(type,
                                      l->substitute(sub),
                                      r->substitute(sub));
}


std::shared_ptr<Term> Relation::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    return std::make_shared<Relation>(type, l->substitute(fs, ex), r->substitute(fs, ex));
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


GiNaC::ex Relation::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    GiNaC::ex newL = l->toGiNaC(subFunSyms, singleVar, sub);
    GiNaC::ex newR = r->toGiNaC(subFunSyms, singleVar, sub);

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
    if (hasNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Relation>(type, newL, newR);
    }
}


std::shared_ptr<Term> Relation::unGinacify() const {
    return std::make_shared<Relation>(type, l->unGinacify(), r->unGinacify());
}


bool Relation::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool Relation::equals(const Relation &term) const {
    return l->equals(*term.l) && r->equals(*term.r);
}


Addition::Addition(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : l(l), r(r) {
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
    return std::make_shared<Addition>(l, r);
}


std::shared_ptr<Term> Addition::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
    return std::make_shared<Addition>(l->evaluateFunction(funDef, addToCost, addToGuard),
                                      r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Addition::evaluateFunction2(const FunctionDefinition &funDef,
                                                  Expression *addToCost,
                                                  ExpressionVector *addToGuard) const {
    return std::make_shared<Addition>(l->evaluateFunction2(funDef, addToCost, addToGuard),
                                      r->evaluateFunction2(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Addition::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                        const TT::ExpressionVector &guard,
                                                        Expression *addToCost,
                                                        bool &modified) const {
    return std::make_shared<Addition>(l->evaluateFunctionIfLegal(funDef, guard, addToCost, modified),
                                      r->evaluateFunctionIfLegal(funDef, guard, addToCost, modified));
}


std::shared_ptr<Term> Addition::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                           TT::ExpressionVector &guard) const {
    return std::make_shared<Addition>(l->moveFunctionSymbolsToGuard(itrs, guard),
                                      r->moveFunctionSymbolsToGuard(itrs, guard));
}


std::shared_ptr<Term> Addition::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                             const std::map<FunctionSymbolIndex,int> &specialCases) const {
    return std::make_shared<Addition>(l->abstractSize(funSyms, specialCases),
                                      r->abstractSize(funSyms, specialCases));
}


std::shared_ptr<Term> Addition::substitute(const Substitution &sub) const {
    return std::make_shared<Addition>(l->substitute(sub),
                                      r->substitute(sub));
}


std::shared_ptr<Term> Addition::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Addition>(l->substitute(sub),
                                      r->substitute(sub));
}


std::shared_ptr<Term> Addition::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    return std::make_shared<Addition>(l->substitute(fs, ex), r->substitute(fs, ex));
}


GiNaC::ex Addition::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    return l->toGiNaC(subFunSyms, singleVar, sub) + r->toGiNaC(subFunSyms, singleVar, sub);
}


Purrs::Expr Addition::toPurrs(int i) const {
    return l->toPurrs(i) + r->toPurrs(i);
}


std::shared_ptr<Term> Addition::ginacify() const {
    if (hasNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Addition>(newL, newR);
    }
}


std::shared_ptr<Term> Addition::unGinacify() const {
    return std::make_shared<Addition>(l->unGinacify(), r->unGinacify());
}


bool Addition::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool Addition::equals(const Addition &term) const {
    return l->equals(*term.l) && r->equals(*term.r);
}


Subtraction::Subtraction(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : l(l), r(r) {
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
    return std::make_shared<Subtraction>(l, r);
}


std::shared_ptr<Term> Subtraction::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
    return std::make_shared<Subtraction>(l->evaluateFunction(funDef, addToCost, addToGuard),
                                         r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Subtraction::evaluateFunction2(const FunctionDefinition &funDef,
                                                     Expression *addToCost,
                                                     ExpressionVector *addToGuard) const {
    return std::make_shared<Subtraction>(l->evaluateFunction2(funDef, addToCost, addToGuard),
                                         r->evaluateFunction2(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Subtraction::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                        const TT::ExpressionVector &guard,
                                                        Expression *addToCost) c,
bool &modifiedonst {
    return std::make_shared<Subtraction>(l->evaluateFunctionIfLegal(funDef, guard, addToCost, modified),
                                         r->evaluateFunctionIfLegal(funDef, guard, addToCost, modified));
}


std::shared_ptr<Term> Subtraction::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                           TT::ExpressionVector &guard) const {
    return std::make_shared<Subtraction>(l->moveFunctionSymbolsToGuard(itrs, guard),
                                      r->moveFunctionSymbolsToGuard(itrs, guard));
}


std::shared_ptr<Term> Subtraction::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                                const std::map<FunctionSymbolIndex,int> &specialCases) const {
    return std::make_shared<Subtraction>(l->abstractSize(funSyms, specialCases),
                                         r->abstractSize(funSyms, specialCases));
}


std::shared_ptr<Term> Subtraction::substitute(const Substitution &sub) const {
    return std::make_shared<Subtraction>(   l->substitute(sub),
                                         r->substitute(sub));
}


std::shared_ptr<Term> Subtraction::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Subtraction>(   l->substitute(sub),
                                         r->substitute(sub));
}


std::shared_ptr<Term> Subtraction::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    return std::make_shared<Subtraction>(l->substitute(fs, ex), r->substitute(fs, ex));
}


GiNaC::ex Subtraction::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    return l->toGiNaC(subFunSyms, singleVar, sub) - r->toGiNaC(subFunSyms, singleVar, sub);
}


Purrs::Expr Subtraction::toPurrs(int i) const {
    return l->toPurrs(i) - r->toPurrs(i);
}


std::shared_ptr<Term> Subtraction::ginacify() const {
    if (hasNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Subtraction>(newL, newR);
    }
}


std::shared_ptr<Term> Subtraction::unGinacify() const {
    return std::make_shared<Subtraction>(l->unGinacify(), r->unGinacify());
}


bool Subtraction::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool Subtraction::equals(const Subtraction &term) const {
    return l->equals(*term.l) && r->equals(*term.r);
}


Multiplication::Multiplication(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : l(l), r(r) {
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
    return std::make_shared<Multiplication>(l, r);
}


std::shared_ptr<Term> Multiplication::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
    return std::make_shared<Multiplication>(l->evaluateFunction(funDef, addToCost, addToGuard),
                                            r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Multiplication::evaluateFunction2(const FunctionDefinition &funDef,
                                                        Expression *addToCost,
                                                        ExpressionVector *addToGuard) const {
    return std::make_shared<Multiplication>(l->evaluateFunction2(funDef, addToCost, addToGuard),
                                            r->evaluateFunction2(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Multiplication::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                        const TT::ExpressionVector &guard,
                                                        Expression *addToCost) cons,
bool &modifiedt {
    return std::make_shared<Multiplication>(l->evaluateFunctionIfLegal(funDef, guard, addToCost, modified),
                                            r->evaluateFunctionIfLegal(funDef, guard, addToCost, modified));
}


std::shared_ptr<Term> Multiplication::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                           TT::ExpressionVector &guard) const {
    return std::make_shared<Multiplication>(l->moveFunctionSymbolsToGuard(itrs, guard),
                                      r->moveFunctionSymbolsToGuard(itrs, guard));
}


std::shared_ptr<Term> Multiplication::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                                   const std::map<FunctionSymbolIndex,int> &specialCases) const {
    return std::make_shared<Multiplication>(l->abstractSize(funSyms, specialCases),
                                            r->abstractSize(funSyms, specialCases));
}


std::shared_ptr<Term> Multiplication::substitute(const Substitution &sub) const {
    return std::make_shared<Multiplication>(      l->substitute(sub),
                                            r->substitute(sub));
}


std::shared_ptr<Term> Multiplication::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Multiplication>(      l->substitute(sub),
                                            r->substitute(sub));
}


std::shared_ptr<Term> Multiplication::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    return std::make_shared<Multiplication>(l->substitute(fs, ex), r->substitute(fs, ex));
}


GiNaC::ex Multiplication::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    return l->toGiNaC(subFunSyms, singleVar, sub) * r->toGiNaC(subFunSyms, singleVar, sub);
}


std::shared_ptr<Term> Multiplication::unGinacify() const {
    return std::make_shared<Multiplication>(l->unGinacify(), r->unGinacify());
}


Purrs::Expr Multiplication::toPurrs(int i) const {
    return l->toPurrs(i) * r->toPurrs(i);
}


std::shared_ptr<Term> Multiplication::ginacify() const {
    if (hasNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Multiplication>(newL, newR);
    }
}


bool Multiplication::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool Multiplication::equals(const Multiplication &term) const {
    return l->equals(*term.l) && r->equals(*term.r);
}


Power::Power(const std::shared_ptr<Term> &l, const std::shared_ptr<Term> &r)
    : l(l), r(r) {
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
    return std::make_shared<Power>(l, r);
}


std::shared_ptr<Term> Power::evaluateFunction(const FunctionDefinition &funDef,
                                                 Expression *addToCost,
                                                 ExpressionVector *addToGuard) const {
    return std::make_shared<Power>(l->evaluateFunction(funDef, addToCost, addToGuard),
                                   r->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Power::evaluateFunction2(const FunctionDefinition &funDef,
                                               Expression *addToCost,
                                               ExpressionVector *addToGuard) const {
    return std::make_shared<Power>(l->evaluateFunction2(funDef, addToCost, addToGuard),
                                   r->evaluateFunction2(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Power::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                        const TT::ExpressionVector &guard,
                                                        Expression *addToC,
                                                        bool &modifiedost) const {
    return std::make_shared<Power>(l->evaluateFunctionIfLegal(funDef, guard, addToCost, modified),
                                   r->evaluateFunctionIfLegal(funDef, guard, addToCost, modified));
}


std::shared_ptr<Term> Power::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                           TT::ExpressionVector &guard) const {
    return std::make_shared<Power>(l->moveFunctionSymbolsToGuard(itrs, guard),
                                      r->moveFunctionSymbolsToGuard(itrs, guard));
}


std::shared_ptr<Term> Power::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                          const std::map<FunctionSymbolIndex,int> &specialCases) const {
    return std::make_shared<Power>(l->abstractSize(funSyms, specialCases),
                                   r->abstractSize(funSyms, specialCases));
}


std::shared_ptr<Term> Power::substitute(const Substitution &sub) const {
    return std::make_shared<Power>(      l->substitute(sub),
                                            r->substitute(sub));
}


std::shared_ptr<Term> Power::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Power>(      l->substitute(sub),
                                            r->substitute(sub));
}


std::shared_ptr<Term> Power::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    return std::make_shared<Power>(l->substitute(fs, ex), r->substitute(fs, ex));
}


GiNaC::ex Power::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    return GiNaC::pow(l->toGiNaC(subFunSyms, singleVar, sub), r->toGiNaC(subFunSyms, singleVar, sub));
}


std::shared_ptr<Term> Power::unGinacify() const {
    return std::make_shared<Power>(l->unGinacify(), r->unGinacify());
}


Purrs::Expr Power::toPurrs(int i) const {
    return Purrs::pwr(l->toPurrs(i), r->toPurrs(i));
}


std::shared_ptr<Term> Power::ginacify() const {
    if (hasNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(toGiNaC());

    } else {
        std::shared_ptr<Term> newL = l->ginacify();
        std::shared_ptr<Term> newR = r->ginacify();
        return std::make_shared<Power>(newL, newR);
    }
}


bool Power::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool Power::equals(const Power &term) const {
    return l->equals(*term.l) && r->equals(*term.r);
}


Factorial::Factorial(const std::shared_ptr<Term> &n)
    : n(n) {
}


void Factorial::traverse(ConstVisitor &visitor) const {
    visitor.visitPre(*this);

    n->traverse(visitor);

    visitor.visitPost(*this);
}


int Factorial::nops() const {
    return 1;
}


std::shared_ptr<Term> Factorial::op(int i) const {
    assert(i >= 0 && i < nops());

    return n;
}


bool Factorial::info(InfoFlag info) const {
    return info == InfoFlag::Factorial;
}


std::shared_ptr<Term> Factorial::copy() const {
    return std::make_shared<Factorial>(n);
}


std::shared_ptr<Term> Factorial::evaluateFunction(const FunctionDefinition &funDef,
                                                  Expression *addToCost,
                                                  ExpressionVector *addToGuard) const {
    return std::make_shared<Factorial>(n->evaluateFunction(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Factorial::evaluateFunction2(const FunctionDefinition &funDef,
                                                   Expression *addToCost,
                                                   ExpressionVector *addToGuard) const {
    return std::make_shared<Factorial>(n->evaluateFunction2(funDef, addToCost, addToGuard));
}


std::shared_ptr<Term> Factorial::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                        const TT::ExpressionVector &guard,
                                                        Expression *addToCost),
                                                        bool &modified const {
    return std::make_shared<Factorial>(n->evaluateFunctionIfLegal(funDef, guard, addToCost, modified));
}


std::shared_ptr<Term> Factorial::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                           TT::ExpressionVector &guard) const {
    return std::make_shared<Factorial>(n->moveFunctionSymbolsToGuard(itrs, guard));
}


std::shared_ptr<Term> Factorial::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                              const std::map<FunctionSymbolIndex,int> &specialCases) const {
    return std::make_shared<Factorial>(n->abstractSize(funSyms, specialCases));
}


std::shared_ptr<Term> Factorial::substitute(const Substitution &sub) const {
    return std::make_shared<Factorial>(n->substitute(sub));
}


std::shared_ptr<Term> Factorial::substitute(const GiNaC::exmap &sub) const {
    return std::make_shared<Factorial>(n->substitute(sub));
}


std::shared_ptr<Term> Factorial::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    return std::make_shared<Factorial>(n->substitute(fs, ex));
}


GiNaC::ex Factorial::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    return GiNaC::factorial(n->toGiNaC(subFunSyms, singleVar, sub));
}


std::shared_ptr<Term> Factorial::unGinacify() const {
    return std::make_shared<Factorial>(n->unGinacify());
}


Purrs::Expr Factorial::toPurrs(int i) const {
    return Purrs::factorial(n->toPurrs(i));
}


std::shared_ptr<Term> Factorial::ginacify() const {
    if (hasNoFunctionSymbols()) {
        return std::make_shared<GiNaCExpression>(toGiNaC());

    } else {
        std::shared_ptr<Term> newN = n->ginacify();
        return std::make_shared<Factorial>(newN);
    }
}


bool Factorial::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool Factorial::equals(const Factorial &term) const {
    return n->equals(*term.n);
}


FunctionSymbol::FunctionSymbol(FunctionSymbolIndex functionSymbol,
                               const std::string &name,
                               const std::vector<std::shared_ptr<Term>> &args)
    : functionSymbol(functionSymbol), name(name), args(args) {
}

FunctionSymbol::FunctionSymbol(FunctionSymbolIndex functionSymbol,
                               const std::string &name,
                               const std::vector<Expression> &args)
    : functionSymbol(functionSymbol), name(name) {
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
    return std::make_shared<FunctionSymbol>(functionSymbol, name, args);
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

    FunctionSymbolIndex funSymbolIndex = funDef.getFunctionSymbol();
    if (funSymbolIndex == functionSymbol) {
        const ::FunctionSymbol &funSymbol = funDef.getITRSProblem().getFunctionSymbol(funSymbolIndex);
        const std::vector<VariableIndex> &vars = funSymbol.getArguments();
        assert(vars.size() == args.size());

        // build the substitution: variable -> passed argument
        debugTerm("\tbuilding substitution");
        Substitution sub;
        for (int i = 0; i < vars.size(); ++i) {
            ExprSymbol var = funDef.getITRSProblem().getGinacSymbol(vars[i]);
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
        return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
    }
}


std::shared_ptr<Term> FunctionSymbol::evaluateFunction2(const FunctionDefinition &funDef,
                                                        Expression *addToCost,
                                                        ExpressionVector *addToGuard) const {
    debugTerm("evaluate: at " << *this);
    // evaluate arguments first
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->evaluateFunction(funDef, addToCost, addToGuard));
    }

    FunctionSymbolIndex funSymbolIndex = funDef.getFunctionSymbol();
    if (funSymbolIndex == functionSymbol) {
        const ::FunctionSymbol &funSymbol = funDef.getITRSProblem().getFunctionSymbol(funSymbolIndex);
        const std::vector<VariableIndex> &vars = funSymbol.getArguments();
        assert(vars.size() == args.size());

        // collect all variables in the rule
        // one should do this before calling this method (in the def class)
        std::vector<ExprSymbol> varsInRule;
        funDef.getDefinition().collectVariables(varsInRule);
        for (const TT::Expression &ex : funDef.getGuard()) {
            ex.collectVariables(varsInRule);
        }
        std::vector<ExprSymbol> varsInCost;;
        funDef.getCost().collectVariables(varsInCost);


        // build the substitution: variable -> passed argument
        debugTerm("\tbuilding substitution");
        Substitution sub;
        for (int i = 0; i < vars.size(); ++i) {
            ExprSymbol var = funDef.getITRSProblem().getGinacSymbol(vars[i]);
            const std::shared_ptr<Term> arg = newArgs[i];

            if (arg->hasFunctionSymbol()) { // move to guard
                VariableIndex chainIndex = funDef.getITRSProblem().addChainingVariable();
                ExprSymbol chainVar = funDef.getITRSProblem().getGinacSymbol(chainIndex);
                sub.emplace(var, Expression(chainVar));
                TT::Expression t = (TT::Expression(chainVar) == TT::Expression(arg));
                addToGuard->push_back(t);

                debugTerm("moving to guard");
                debugTerm("(adding " << t << ")");
                debugTerm("\t" << var << "\\" << chainVar);

            } else {
                sub.emplace(var, Expression(newArgs[i]));

                debugTerm("not moving to guard");
                debugTerm("\t" << var << "\\" << *newArgs[i]);
            }
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
        return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
    }
}


std::shared_ptr<Term> FunctionSymbol::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                              const TT::ExpressionVector &guard,
                                                              Expression *addToCost,
                                                              bool &modified) const {
    debugTerm("evaluateIfLegal: at " << *this);
    // evaluate arguments first
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->evaluateFunctionIfLegal(funDef, guard, addToCost));
    }

    FunctionSymbolIndex funSymbolIndex = funDef.getFunctionSymbol();
    if (funSymbolIndex == functionSymbol) {
        const ::FunctionSymbol &funSymbol = funDef.getITRSProblem().getFunctionSymbol(funSymbolIndex);
        const std::vector<VariableIndex> &vars = funSymbol.getArguments();
        assert(vars.size() == args.size());

        // build the substitution: variable -> passed argument
        debugTerm("\tbuilding substitution");
        Substitution sub;
        for (int i = 0; i < vars.size(); ++i) {
            ExprSymbol var = funDef.getITRSProblem().getGinacSymbol(vars[i]);
            sub.emplace(var, Expression(newArgs[i]));
            debugTerm("\t" << var << "\\" << *newArgs[i]);
        }


        // check if it is legal to apply this function definition,
        // i.e., guard => funDef.guard [sub]
        debugTerm("TERM LHS:");
        std::vector<::Expression> lhs;
        for (const TT::Expression &ex : guard) {
            lhs.push_back(ex.toGiNaC(true));
        }
        for (const TT::Expression &ex : funDef.getGuard()) {
            ::Expression rhs = ex.substitute(sub).toGiNaC(true);
            debugTerm("TERM RHS: " << rhs);

            if (!Z3Toolbox::checkTautologicImplication(lhs, rhs)) {
                debugTerm("FALSE");
                return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
            }
            debugTerm("TRUE");
        }

        modified = true;

        // apply the sub
        if (addToCost) {
            *addToCost = *addToCost + funDef.getCost().substitute(sub);
        }

        debugTerm("funDef: " << funDef.getDefinition());
        std::shared_ptr<Term> result = funDef.getDefinition().getTermTree()->substitute(sub);
        debugTerm("result: " << *result);

        return result;

    } else {
        return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
    }
}


std::shared_ptr<Term> FunctionSymbol::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                                 TT::ExpressionVector &guard) const {
    debugTerm("moveFunctionSymbolsToGuard: at " << *this);
    for (const std::shared_ptr<Term> &arg : args) {
        assert(!arg->hasFunctionSymbol());
    }

    VariableIndex vi = itrs.addChainingVariable();
    ExprSymbol var = itrs.getGinacSymbol(vi);
    TT::Expression thisTerm = TT::Expression(std::make_shared<FunctionSymbol>(functionSymbol, name, args))
    guard.push_back(var == thisTerm);

    return std::make_shared<GiNaCExpression>(var);
}


std::shared_ptr<Term> FunctionSymbol::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                                                   const std::map<FunctionSymbolIndex,int> &specialCases) const {
    auto it = specialCases.find(functionSymbol);
    if (it != specialCases.end()) {
        return std::make_shared<GiNaCExpression>(GiNaC::numeric(it->second));
    }

    // abstract arguments first
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->abstractSize(funSyms, specialCases));
    }

    if (funSyms.count(functionSymbol) == 1) {
        std::shared_ptr<Term> res = std::make_shared<GiNaCExpression>(GiNaC::numeric(1));
        for (const std::shared_ptr<Term> &arg : newArgs) {
            res = std::make_shared<Addition>(res, arg);
        }

        return res;

    } else {
        return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
    }
}


FunctionSymbolIndex FunctionSymbol::getFunctionSymbol() const {
    return functionSymbol;
}


std::string FunctionSymbol::getName() const {
    return name;
}


const std::vector<std::shared_ptr<Term>>& FunctionSymbol::getArguments() const {
    return args;
}


std::shared_ptr<Term> FunctionSymbol::substitute(const Substitution &sub) const {
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->substitute(sub));
    }

    return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
}


std::shared_ptr<Term> FunctionSymbol::substitute(const GiNaC::exmap &sub) const {
    std::vector<std::shared_ptr<Term>> newArgs;
    for (const std::shared_ptr<Term> &arg : args) {
        newArgs.push_back(arg->substitute(sub));
    }

    return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
}


std::shared_ptr<Term> FunctionSymbol::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    if (functionSymbol == fs) {
        return ex;

    } else {
        std::vector<std::shared_ptr<Term>> newArgs;
        for (const std::shared_ptr<Term> &arg : args) {
            newArgs.push_back(arg->substitute(fs, ex));
        }

        return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
    }
}


GiNaC::ex FunctionSymbol::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    if (subFunSyms) {
        if (singleVar != nullptr) {
            return *singleVar;
        } else if (sub != nullptr) {
            for (auto const &pair : *sub) {
                if (pair.first.getTermTree()->equals(*this)) {
                    return pair.second;
                }
            }

            ExprSymbol symbol;
            sub->emplace_back(Expression(copy()), symbol);
            return symbol;

        } else {
            return ExprSymbol();
        }

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
        if (term->hasNoFunctionSymbols()) {
            newArgs.push_back(std::make_shared<GiNaCExpression>(term->toGiNaC()));

        } else {
            newArgs.push_back(term->ginacify());
        }
    }

    return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
}


std::shared_ptr<Term> FunctionSymbol::unGinacify() const {
    std::vector<std::shared_ptr<Term>> newArgs;

    for (const std::shared_ptr<Term> &term : args) {
        newArgs.push_back(term->unGinacify());
    }

    return std::make_shared<FunctionSymbol>(functionSymbol, name, newArgs);
}


bool FunctionSymbol::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool FunctionSymbol::equals(const FunctionSymbol &term) const {
    if (functionSymbol != term.functionSymbol) {
        return false;
    }

    assert(args.size() == term.args.size());
    for (int i = 0; i < args.size(); ++i) {
        if (!args[i]->equals(*term.args[i])) {
            return false;
        }
    }

    return true;
}


GiNaCExpression::GiNaCExpression(const GiNaC::ex &expr)
    : expression(expr) {
}


void GiNaCExpression::traverse(ConstVisitor &visitor) const {
    visitor.visit(*this);
}


int GiNaCExpression::nops() const {
    return expression.nops();
}


std::shared_ptr<Term> GiNaCExpression::op(int i) const {
    assert(i >= 0 && i < expression.nops());

    return std::make_shared<GiNaCExpression>(expression.op(i));
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

        case InfoFlag::Variable:
            return GiNaC::is_a<GiNaC::symbol>(expression);

        default:
            return false;
    }
}


std::shared_ptr<Term> GiNaCExpression::copy() const {
    return std::make_shared<GiNaCExpression>(expression);
}


std::shared_ptr<Term> GiNaCExpression::evaluateFunction(const FunctionDefinition &funDef,
                                                        Expression *addToCost,
                                                        ExpressionVector *addToGuard) const {
    return copy();
}


std::shared_ptr<Term> GiNaCExpression::evaluateFunction2(const FunctionDefinition &funDef,
                                                         Expression *addToCost,
                                                         ExpressionVector *addToGuard) const {
    return copy();
}


std::shared_ptr<Term> GiNaCExpression::evaluateFunctionIfLegal(const FunctionDefinition &funDef,
                                                               const TT::ExpressionVector &guard,
                                                               Expression *addToCost,
                                                               bool &modified) const {
    return copy();
}


std::shared_ptr<Term> GiNaCExpression::moveFunctionSymbolsToGuard(ITRSProblem &itrs,
                                                                  TT::ExpressionVector &guard) const {
    return copy();
}


std::shared_ptr<Term> GiNaCExpression::abstractSize(const std::set<FunctionSymbolIndex> &funSyms,
                            const std::map<FunctionSymbolIndex,int> &specialCases) const {
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
    return std::make_shared<GiNaCExpression>(expression.subs(sub));
}


std::shared_ptr<Term> GiNaCExpression::substitute(FunctionSymbolIndex fs, const std::shared_ptr<Term> ex) const {
    return copy();
}


GiNaC::ex GiNaCExpression::toGiNaC(bool subFunSyms,const ExprSymbol *singleVar, FunToVarSub *sub) const {
    return expression;
}


Purrs::Expr GiNaCExpression::toPurrs(int i) const {
    return Purrs::Expr::fromGiNaC(expression);
}


std::shared_ptr<Term> GiNaCExpression::ginacify() const {
    return std::make_shared<GiNaCExpression>(expression);
}


std::shared_ptr<Term> GiNaCExpression::unGinacify() const {
    return fromGiNaC(expression);
}


bool GiNaCExpression::equals(const Term &term) const {
    return term.equals(*this); // double dispatch
}


bool GiNaCExpression::equals(const GiNaCExpression &term) const {
    return expression.is_equal(term.expression);
}

};