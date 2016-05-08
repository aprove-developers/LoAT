#include "recursion.h"

#include <purrs.hh>

#include "guardtoolbox.h"
#include "recursiongraph.h"
#include "z3toolbox.h"

namespace GT = GuardToolbox;
namespace Z3T = Z3Toolbox;

Recursion::Recursion(const ITRSProblem &itrs,
                     FunctionSymbolIndex funSymbolIndex,
                     std::set<const RightHandSide*> &rightHandSides,
                     TT::Expression &result,
                     TT::Expression &cost,
                     TT::ExpressionVector &guard)
    : itrs(itrs), funSymbolIndex(funSymbolIndex), rightHandSides(rightHandSides),
      result(result), cost(cost), guard(guard) {
}

bool Recursion::solve() {
    const FunctionSymbol& funSymbol = itrs.getFunctionSymbol(funSymbolIndex);
    // TODO: Support multiple variables
    if (funSymbol.getArguments().size() != 1) {
        return false;
    }

    critVar = funSymbol.getArguments()[0];
    critVarGiNaC = itrs.getGinacSymbol(critVar);

    recursion = nullptr;
    for (const RightHandSide *rhs : rightHandSides) {
        std::set<FunctionSymbolIndex> funSymbols = std::move(rhs->term.getFunctionSymbols());

        if (funSymbols.size() == 1 && funSymbols.count(funSymbolIndex) == 1) {
            debugPurrs("Found recursion: " << *rhs);
            ExprSymbolSet vars = std::move(rhs->term.getVariables());

            if (vars.size() == 1 && vars.count(critVarGiNaC) == 1) {
                // TODO avoid nested calls
                debugPurrs("Recursion is suitable");
                recursion = rhs;
                rightHandSides.erase(rhs);
                break;
            }
        }
    }

    // No valid recursion found
    if (recursion == nullptr) {
        return false;
    }

    // Identify potential base cases
    for (auto it = rightHandSides.begin(); it != rightHandSides.end();) {
        if ((*it)->term.containsNoFunctionSymbols()) {
            debugPurrs("Potential base case: " << **it);
            ++it;

        } else {
            it = rightHandSides.erase(it);
        }
    }

    if (!findBaseCases()) {
        debugPurrs("Found no usable base cases");
        return false;
    }

    if (!baseCasesAreSufficient()) {
        debugPurrs("Base cases are not sufficient");
        return false;
    }

    debugPurrs("===Solving recursion===");
    GiNaC::exmap varSub;
    varSub.emplace(critVarGiNaC, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
    Purrs::Expr recurrence = recursion->term.substitute(varSub).toPurrs();

    std::map<Purrs::index_type,Purrs::Expr> baseCasesPurrs;
    for (auto const &pair : baseCases) {
        baseCasesPurrs.emplace(pair.first, pair.second->term.toPurrs());
    }

    if (!solve(recurrence, baseCasesPurrs)) {
        debugPurrs("Could not solve recurrence");
        return false;
    }

    GiNaC::exmap varReSub;
    varReSub.emplace(Purrs::Expr(Purrs::Recurrence::n).toGiNaC(), critVarGiNaC);
    result = TT::Expression(itrs, recurrence.toGiNaC().subs(varReSub));

    debugPurrs("===Solving cost===");
    TT::Expression costRecurrence = recursion->cost;
    for (const TT::Expression &update : recursion->term.getUpdates()) {
        std::vector<TT::Expression> updateAsVector;
        updateAsVector.push_back(update);
        costRecurrence = costRecurrence + TT::Expression(itrs, funSymbolIndex, updateAsVector);
    }
    recurrence = costRecurrence.substitute(varSub).toPurrs();

    baseCasesPurrs.clear();
    for (auto const &pair : baseCases) {
        baseCasesPurrs.emplace(pair.first, pair.second->cost.toPurrs());
    }

    if (!solve(recurrence, baseCasesPurrs)) {
        debugPurrs("Could not solve recurrence");
        return false;
    }

    cost = TT::Expression(itrs, recurrence.toGiNaC().subs(varReSub));

    debugPurrs("===Constructing guard===");

    Expression toGuard = critVarGiNaC >= 0;
    guard.push_back(TT::Expression(itrs, toGuard));

    debugPurrs("===Resulting rhs===");
    debugPurrs("definition: " << result);
    debugPurrs("cost: " << cost);
    debugPurrs("guard:");
    for (const TT::Expression &ex : guard) {
        debugPurrs(ex);
    }

    return true;
}


bool Recursion::findBaseCases() {
    debugPurrs("===Searching for base cases===");
    for (auto it = rightHandSides.begin(); it != rightHandSides.end();) {
        std::vector<Expression> query;
        for (const TT::Expression &ex : (*it)->guard) {
            assert(ex.containsNoFunctionSymbols());
            query.push_back(ex.toGiNaC());
        }

        Z3VariableContext context;
        z3::model model(context, Z3_model());
        z3::check_result result;
        result = Z3Toolbox::checkExpressionsSAT(query, context, &model);

        debugPurrs("Examining " << **it << " as a potential base case");
        if (result == z3::sat) {
            Expression value = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(critVarGiNaC, context));
            if (value.info(GiNaC::info_flags::integer)
                && value.info(GiNaC::info_flags::nonnegative)) {
                // TODO Check range
                Purrs::index_type asUInt = GiNaC::ex_to<GiNaC::numeric>(value).to_int();
                if (baseCases.count(asUInt) == 0) {
                    // TODO Try to derive more base cases from one rhs
                    debugPurrs("is a potential base case for " << critVarGiNaC << " = " << asUInt);
                    baseCases.emplace(asUInt, *it);
                    it = rightHandSides.erase(it);
                    continue;

                } else {
                    debugPurrs("Discarding potential base case for " << critVarGiNaC << " = " << asUInt);
                }

            } else {
                debugPurrs("Error, " << value << " is not a natural number");
            }

        } else {
            debugPurrs("Z3 was not sat");
        }
        ++it;
    }

    return !baseCases.empty();
}


bool Recursion::baseCasesAreSufficient() {
    debugPurrs("===Checking if base cases are sufficient===");
    std::vector<TT::Expression> updates = std::move(recursion->term.getUpdates());

    for (const TT::Expression &update : updates) {
        debugPurrs("Update: " << update);
        assert(update.containsNoFunctionSymbols());
        GiNaC::exmap updateSub;
        updateSub.emplace(critVarGiNaC, update.toGiNaC());

        std::vector<std::vector<Expression>> queryRhs; // disjunction of conjunctions
        debugPurrs("RHS:");
        for (auto const &pair : baseCases) {
            debugPurrs("OR (updated base case guard)");
            std::vector<Expression> updatedGuard;
            for (const TT::Expression &ex : pair.second->guard) {
                assert(ex.containsNoFunctionSymbols());
                updatedGuard.push_back(ex.toGiNaC().subs(updateSub));
                debugPurrs("\tAND " << updatedGuard.back());
            }
            Expression forceCritVar(critVarGiNaC == pair.first);
            updatedGuard.push_back(forceCritVar.subs(updateSub));
            debugPurrs("\tAND (forcing critVar) " << updatedGuard.back());

            queryRhs.push_back(std::move(updatedGuard));
        }

        std::vector<Expression> queryLhs; // conjunction
        for (const TT::Expression &ex : recursion->guard) {
            assert(ex.containsNoFunctionSymbols());
            queryLhs.push_back(ex.toGiNaC());
        }

        for (const TT::Expression &negateEx : recursion->guard) {
            queryLhs.push_back(GT::negateLessEqualInequality(GT::makeLessEqual(negateEx.toGiNaC().subs(updateSub))));

            debugPurrs("LHS:");
            for (const Expression &ex : queryLhs) {
                debugPurrs("AND " << ex);
            }

            if (!Z3T::checkTautologicImplication(queryLhs, queryRhs)) {
                debugPurrs("FALSE");
                return false;
            }

            debugPurrs("TRUE");

            queryLhs.pop_back();
        }
    }

    return true;
}


bool Recursion::solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc) {
    debugPurrs("Solving recurrence: " << recurrence);
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(recurrence);
        debugPurrs("base cases:");
        for (auto const &pair : bc) {
            debugPurrs(critVarGiNaC << " = " << pair.first << " is " << pair.second);
        }
        rec.set_initial_conditions(bc);

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
        debugPurrs("Purrs failed (not SUCCESS)");
            return false;
        }

        rec.exact_solution(exact);

    } catch (...) {
        debugPurrs("Purrs failed (Exception)");
        return false;
    }

    debugPurrs("solution: " << exact);

    recurrence = exact;
    return true;
}
