#include "recursion.h"

#include <purrs.hh>

#include "recursiongraph.h"

bool Recursion::solve(const ITRSProblem &itrs,
                      const FunctionSymbol &funSymbol,
                      std::set<RightHandSide*> rightHandSides,
                      Expression &result,
                      Expression &cost,
                      TermVector &guard) {
    class VarSubVisitor : public TT::Term::Visitor {
    public:
        VarSubVisitor(const ITRSProblem &itrs) {
        }

        void visit(TT::GiNaCExpression &expr) override {
            GiNaC::exmap sub;
            for (const ExprSymbol &sym : Expression(expr.getExpression()).getVariables()) {
                sub.emplace(sym, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
            }

            expr.setExpression(expr.getExpression().subs(sub));
        }
    };
    VariableIndex critVar = funSymbol.getArguments()[0];
    RightHandSide rhs = *(*(rightHandSides.begin()));
    debugPurrs(rhs);

    VarSubVisitor vis(itrs);
    rhs.term = rhs.term->copy();
    debugPurrs("rhs after copy: " << rhs);
    rhs.term->traverse(vis);

    Purrs::Expr recurrence = rhs.term->toPurrs();
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(recurrence);
        rec.set_initial_conditions({ {0, 0} }); //costs for no iterations are hopefully 0

        debugPurrs("recurrence: " << recurrence);

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
            return false;
        }

        rec.exact_solution(exact);
    } catch (...) {
        debugPurrs("Purrs failed on x(n) = " << recurrence << " with initial x(0)=0");
        return false;
    }

    result = exact.toGiNaC();
    GiNaC::exmap sub;
    sub.emplace(Purrs::Expr(Purrs::Recurrence::n).toGiNaC(), itrs.getGinacSymbol(critVar));
    result = result.subs(sub);

    cost = result + 1;

    rhs.term = std::make_shared<TT::GiNaCExpression>(itrs, result);

    Expression toGuard = itrs.getGinacSymbol(critVar) >= 0;
    guard.push_back(TT::Term::fromGiNaC(itrs, toGuard));

    debugPurrs("definition: " << result);
    debugPurrs("cost: " << cost);
    debugPurrs("guard:");
    for (const std::shared_ptr<TT::Term> &term : guard) {
        debugPurrs(*term);
    }

    return true;
}