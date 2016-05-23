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
      result(result), cost(cost), guard(guard), funSymbol(itrs.getFunctionSymbol(funSymbolIndex)) {
}

bool Recursion::solve() {
    if (!findRecursion()) {
        debugPurrs("No suitable recursion found");
        return false;
    }

    if (realVars.size() == 1) {
        realVarIndex = *realVars.begin();
        realVar = funSymbol.getArguments()[realVarIndex];
        realVarGiNaC = itrs.getGinacSymbol(realVar);

        // Identify potential base cases
        for (auto it = rightHandSides.begin(); it != rightHandSides.end();) {
            if ((*it)->term.hasNoFunctionSymbols()) {
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
        varSub.emplace(realVarGiNaC, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
        Purrs::Expr recurrence = recursion->term.substitute(varSub).toPurrs(realVarIndex);

        std::map<Purrs::index_type,Purrs::Expr> baseCasesPurrs;
        for (auto const &pair : baseCases) {
            baseCasesPurrs.emplace(pair.first, pair.second->term.toPurrs());
        }

        if (!solve(recurrence, baseCasesPurrs)) {
            debugPurrs("Could not solve recurrence");
            return false;
        }

        GiNaC::exmap varReSub;
        varReSub.emplace(Purrs::Expr(Purrs::Recurrence::n).toGiNaC(), realVarGiNaC);
        result = TT::Expression(itrs, recurrence.toGiNaC().subs(varReSub));


        debugPurrs("===Constructing guard===");
        debugPurrs("using guard of recursion:");
        TT::ExpressionVector preEvaluatedGuard;
        for (const TT::Expression &ex : recursion->guard) {
            debugPurrs(ex);
            guard.push_back(ex);
            preEvaluatedGuard.push_back(ex);
        }

        // We already have the definition for this function symbol
        // Evaluate all occurences in the guard and the cost
        TT::Expression dummy(itrs, GiNaC::numeric(0));
        TT::FunctionDefinition funDef(funSymbolIndex, result, dummy, guard);

        debugPurrs("Pre-evaluated guard:");
        for (int i = 0; i < preEvaluatedGuard.size(); ++i) {
            preEvaluatedGuard[i] = preEvaluatedGuard[i].evaluateFunction(funDef, nullptr, nullptr).ginacify();
            debugPurrs(preEvaluatedGuard[i]);
        }
        // Update funDef
        // TODO optimze
        funDef = TT::FunctionDefinition(funSymbolIndex, result, dummy, preEvaluatedGuard);

        debugPurrs("Evaluated guard:");
        for (int i = 0; i < guard.size(); ++i) {
            TT::Expression temp = std::move(guard[i]);
            guard[i] = std::move(temp.evaluateFunction(funDef, nullptr, &guard).ginacify());

            debugPurrs(guard[i]);
        }

        int oldSize = guard.size();
        TT::Expression costRecurrence = recursion->cost.evaluateFunction(funDef, nullptr, &guard).ginacify();
        for (int i = oldSize; i < guard.size(); ++i) {
            guard[i] = guard[i].ginacify();
        }
        debugPurrs("After evaluating cost:");
        for (const TT::Expression &ex : guard) {
            debugPurrs(ex);
        }


        debugPurrs("===Solving cost===");
        for (const TT::Expression &funApp : recursion->term.getFunctionApplications()) {
            TT::Expression update = funApp.op(realVarIndex);
            std::vector<TT::Expression> updateAsVector;
            updateAsVector.push_back(update);
            costRecurrence = costRecurrence + TT::Expression(itrs, funSymbolIndex, updateAsVector);
        }
        recurrence = costRecurrence.substitute(varSub).toPurrs(0);

        baseCasesPurrs.clear();
        for (auto const &pair : baseCases) {
            baseCasesPurrs.emplace(pair.first, pair.second->cost.toPurrs());
        }

        if (!solve(recurrence, baseCasesPurrs)) {
            debugPurrs("Could not solve recurrence");
            return false;
        }

        cost = TT::Expression(itrs, recurrence.toGiNaC().subs(varReSub));


        debugPurrs("===Resulting rhs===");
        debugPurrs("definition: " << result);
        debugPurrs("cost: " << cost);
        debugPurrs("guard:");
        for (const TT::Expression &ex : guard) {
            debugPurrs(ex);
        }

        return true;

    } else {
        // TODO handle two real vars

        return false;
    }
}


bool Recursion::findRecursion() {
    debugPurrs("===Finding recursion===");
    recursion = nullptr;

    for (const RightHandSide *rhs : rightHandSides) {
        std::set<FunctionSymbolIndex> funSymbols = std::move(rhs->term.getFunctionSymbols());

        if (funSymbols.size() == 1 && funSymbols.count(funSymbolIndex) == 1) {
            debugPurrs("Found recursion: " << *rhs);

            // Check if there are any function symbols besides funSymbol in the cost/guard
            funSymbols = std::move(rhs->term.getFunctionSymbols());
            if (funSymbols.size() > 0) {
                if (funSymbols.size() > 1 || funSymbols.count(funSymbolIndex) == 0) {
                    debugPurrs("cost contains an alien function symbol");
                    continue;
                }
            }

            for (const TT::Expression &ex : rhs->guard) {
                funSymbols = std::move(ex.getFunctionSymbols());

                if (funSymbols.size() > 0) {
                    if (funSymbols.size() > 1 || funSymbols.count(funSymbolIndex) == 0) {
                        debugPurrs("guard contains an alien function symbol");
                    }
                }
            }

            if (findRealVars(rhs->term)) {
                debugPurrs("Recursion is suitable");
                recursion = rhs;
                rightHandSides.erase(rhs);
                break;
            }
        }
    }

    return recursion != nullptr;
}


bool Recursion::findRealVars(const TT::Expression &term) {
    debugPurrs("===Finding real recursion variables===");
    const std::vector<VariableIndex> &vars = funSymbol.getArguments();

    std::vector<TT::Expression> funApps = std::move(term.getFunctionApplications());
    for (int i = 0; i < vars.size(); ++i) {
        ExprSymbol var = itrs.getGinacSymbol(vars[i]);
        debugPurrs("variable: " << var);

        for (const TT::Expression &funApp : funApps) {
            debugPurrs("function application: " << funApp);
            assert(funApp.nops() == vars.size());

            TT::Expression update = funApp.op(i);
            debugPurrs("update: " << update);
            if (!update.hasNoFunctionSymbols()) {
                debugPurrs("Update contains function symbol, cannot continue");
                return false;
            }

            if (var != update.toGiNaC()) {
                debugPurrs("real");
                realVars.insert(i);
            }
        }
    }

    return realVars.size() >= 1 && realVars.size() <= 2;
}


bool Recursion::findBaseCases() {
    debugPurrs("===Searching for base cases===");
    for (auto it = rightHandSides.begin(); it != rightHandSides.end();) {
        std::vector<Expression> query;
        for (const TT::Expression &ex : (*it)->guard) {
            assert(ex.hasNoFunctionSymbols());
            query.push_back(ex.toGiNaC());
        }

        Z3VariableContext context;
        z3::model model(context, Z3_model());
        z3::check_result result;
        result = Z3Toolbox::checkExpressionsSAT(query, context, &model);

        debugPurrs("Examining " << **it << " as a potential base case");
        if (result == z3::sat) {
            Expression value = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(realVarGiNaC, context));
            if (value.info(GiNaC::info_flags::integer)
                && value.info(GiNaC::info_flags::nonnegative)) {
                // TODO Check range
                Purrs::index_type asUInt = GiNaC::ex_to<GiNaC::numeric>(value).to_int();
                if (baseCases.count(asUInt) == 0) {
                    // TODO Try to derive more base cases from one rhs
                    debugPurrs("is a potential base case for " << realVarGiNaC << " = " << asUInt);
                    baseCases.emplace(asUInt, *it);
                    it = rightHandSides.erase(it);
                    continue;

                } else {
                    debugPurrs("Discarding potential base case for " << realVarGiNaC << " = " << asUInt);
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
    std::vector<TT::Expression> funApps = std::move(recursion->term.getFunctionApplications());

    for (const TT::Expression &funApp : funApps) {
        TT::Expression update = funApp.op(realVarIndex);
        debugPurrs("Update: " << update);
        assert(update.hasNoFunctionSymbols());
        GiNaC::exmap updateSub;
        updateSub.emplace(realVarGiNaC, update.toGiNaC());

        std::vector<std::vector<Expression>> queryRhs; // disjunction of conjunctions
        debugPurrs("RHS:");
        for (auto const &pair : baseCases) {
            debugPurrs("OR (updated base case guard)");
            std::vector<Expression> updatedGuard;
            for (const TT::Expression &ex : pair.second->guard) {
                if (!ex.hasNoFunctionSymbols()) {
                    debugPurrs("Warning: guard contains function symbol, substituting by variable");
                }

                // Using toGiNaC(true) to substitute function symbols by variables
                updatedGuard.push_back(ex.toGiNaC(true).subs(updateSub));
                debugPurrs("\tAND " << updatedGuard.back());
            }
            Expression forceCritVar(realVarGiNaC == pair.first);
            updatedGuard.push_back(forceCritVar.subs(updateSub));
            debugPurrs("\tAND (forcing realVar) " << updatedGuard.back());

            queryRhs.push_back(std::move(updatedGuard));
        }

        std::vector<Expression> queryLhs; // conjunction
        for (const TT::Expression &ex : recursion->guard) {
            if (!ex.hasNoFunctionSymbols()) {
                debugPurrs("Warning: guard contains function symbol, substituting by variable");
            }

            // Using toGiNaC(true) to substitute function symbols by variables

            queryLhs.push_back(ex.toGiNaC(true));
        }

        for (const TT::Expression &negateEx : recursion->guard) {
            // Using toGiNaC(true) to substitute function symbols by variables
            queryLhs.push_back(GT::negateLessEqualInequality(GT::makeLessEqual(negateEx.toGiNaC(true).subs(updateSub))));

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
            debugPurrs(realVarGiNaC << " = " << pair.first << " is " << pair.second);
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
