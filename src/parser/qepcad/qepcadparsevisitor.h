#ifndef QEPCADPARSEVISITOR_H
#define QEPCADPARSEVISITOR_H

#include "qepcadVisitor.h"
#include "../../util/exceptions.hpp"
#include "../../expr/boolexpr.hpp"
#include "../../its/variablemanager.hpp"

class QepcadParseVisitor: qepcadVisitor
{
    enum BinOp {
        Minus, Exp, Plus
    };

    EXCEPTION(ParseError, CustomException);

    VariableManager &varMan;

    QepcadParseVisitor(VariableManager &varMan);

    virtual antlrcpp::Any visitMain(qepcadParser::MainContext *ctx) override;
    virtual antlrcpp::Any visitExpr(qepcadParser::ExprContext *ctx) override;
    virtual antlrcpp::Any visitBinop(qepcadParser::BinopContext *ctx) override;
    virtual antlrcpp::Any visitFormula(qepcadParser::FormulaContext *ctx) override;
    virtual antlrcpp::Any visitLit(qepcadParser::LitContext *ctx) override;
    virtual antlrcpp::Any visitBoolop(qepcadParser::BoolopContext *ctx) override;
    virtual antlrcpp::Any visitRelop(qepcadParser::RelopContext *ctx) override;

public:

    static BoolExpr parse(std::string str, VariableManager &varMan);

};

#endif
