#include "qepcad.hpp"

option<BoolExpr> Qepcad::qe(const QuantifiedFormula &qf, VariableManager &varMan) {
    void* topOfTheStack = 0;
    Word ac, Fs, V;
    char **av;
    ARGSACLIB(0,0,&ac,&av);
    BEGINSACLIB((Word *)topOfTheStack);
    BEGINQEPCAD(ac,av);
    option<std::string> input = qf.toQepcad();
    if (input) {
        std::stringstream in(input.get());
        std::ostringstream out;
        PushInputContext(in);
        PushOutputContext(out);
        INPUTRD(&Fs, &V);
        QepcadCls cad(V, Fs);
        cad.CADautoConst();
        cad.GETDEFININGFORMULA();
        std::string res = out.str();
        BoolExpr e = QepcadParseVisitor::parse(res, varMan);
        ENDQEPCAD();
        ENDSACLIB(SAC_FREEMEM);
        free(av);
        return e;
    }
    return {};
}
