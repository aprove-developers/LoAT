#include "qepcad.hpp"
#include <qepcad/db/CAPolicy.h>
#include <qepcad/db/OriginalPolicy.h>

void Qepcad::init() {
    static Word ac;
    static void* topOfTheStack = 0;
    static char *a, *b;
    static char **args = &a;
    static char **av = &b;
    ARGSACLIB(0,args,&ac,&av);
    BEGINSACLIB((Word *)topOfTheStack);
    BEGINQEPCADLIB();
}

void Qepcad::exit() {
    ENDSACLIB(SAC_FREEMEM);
}

option<BoolExpr> Qepcad::qe(const QuantifiedFormula &qf, VariableManager &varMan) {
    Word Fs, V, freeVars, t;
    option<QuantifiedFormula::QepcadIn> input = qf.toQepcad();
    std::string instr = input->variables + " " + std::to_string(input->freeVariables) + " " + input->formula;
    option<BoolExpr> res;
    if (input) {
        std::stringstream in(instr);
        std::ostringstream out;
        PushInputContext(in);
        PushOutputContext(out);
        VLREADR(&V,&t);
        assert(t==1);
        GREADR(&freeVars,&t);
        assert(t==1);
        FREADR(V,freeVars,&Fs,&t);
        assert(t==1);
        QepcadCls cad(V, Fs);
        cad.CADautoConst();
        Word F = cad.GETDEFININGFORMULA();
        QFFWR(V, F);
        res = QepcadParseVisitor::parse(out.str(), varMan);
        if (res) {
            res = res.get()->simplify();
        }
    }
    return res;
}
