#include "recursion.h"

#include <purrs.hh>

#include "recursiongraph.h"

bool Recursion::solve(const ITRSProblem &itrs,
                      const FunctionSymbol &funSymbol,
                      std::set<RightHandSide*> rightHandSides,
                      Expression &result,
                      Expression &cost,
                      TT::ExpressionVector &guard) {
    VariableIndex critVar = funSymbol.getArguments()[0];
    RightHandSide rhs = *(*(rightHandSides.begin()));
    debugPurrs(rhs);

    GiNaC::exmap varSub;
    for (const ExprSymbol &var : rhs.term.getVariables()) {
        debugPurrs("varsub: " << var << "\\" << "n");
        varSub.emplace(var, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
    }
    rhs.term = rhs.term.substitute(varSub);

    Purrs::Expr recurrence = rhs.term.toPurrs();
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

    rhs.term = TT::Expression(itrs, result);

    Expression toGuard = itrs.getGinacSymbol(critVar) >= 0;
    guard.push_back(TT::Expression(itrs, toGuard));

    debugPurrs("definition: " << result);
    debugPurrs("cost: " << cost);
    debugPurrs("guard:");
    for (const TT::Expression &ex : guard) {
        debugPurrs(ex);
    }

    return true;
}