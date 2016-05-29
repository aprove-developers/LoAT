#include "recursion.h"

#include <purrs.hh>

#include "guardtoolbox.h"
#include "recursiongraph.h"
#include "z3toolbox.h"

namespace GT = GuardToolbox;
namespace Z3T = Z3Toolbox;

Recursion::Recursion(const ITRSProblem &itrs,
                     FunctionSymbolIndex funSymbolIndex,
                     const std::set<const RightHandSide*> &rightHandSides,
                     std::set<const RightHandSide*> &wereUsed,
                     std::vector<RightHandSide> &result)
    : itrs(itrs), funSymbolIndex(funSymbolIndex), rightHandSides(rightHandSides),
      wereUsed(wereUsed), result(result),
      funSymbol(itrs.getFunctionSymbol(funSymbolIndex)) {
}

bool Recursion::solve() {
    if (!findRecursions()) {
        debugRecursion("No suitable recursion found");
        return false;
    }

    if (realVars.size() == 1) {
        realVarIndex = *realVars.begin();
        realVar = funSymbol.getArguments()[realVarIndex];
        realVarGiNaC = itrs.getGinacSymbol(realVar);

        if (!findBaseCases()) {
            debugRecursion("Found no usable base cases");
            return false;
        }

        bool solved = false;
        for (const RightHandSide *rhs : recursions) {
            recursion = rhs;
            if (solveRecursionInOneVar()) {
                wereUsed.insert(rhs);
                solved = true;
            }
        }

        if (solved) {
            for (auto const &pair : baseCases) {
                wereUsed.insert(pair.second);
            }
        }

        return solved;

    } else {
        // TODO handle two real vars

        return false;
    }
}


bool Recursion::solveRecursionInOneVar() {
        debugRecursion("===Trying to solve recursion===");
        debugRecursion("Recursion: " << *recursion);

        if (!baseCasesAreSufficient()) {
            debugRecursion("Base cases are not sufficient");
            return false;
        }

        debugRecursion("===Solving recursion===");
        GiNaC::exmap varSub;
        varSub.emplace(realVarGiNaC, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
        Purrs::Expr recurrence = recursion->term.substitute(varSub).toPurrs(realVarIndex);

        std::map<Purrs::index_type,Purrs::Expr> baseCasesPurrs;
        for (auto const &pair : baseCases) {
            baseCasesPurrs.emplace(pair.first, pair.second->term.toPurrs());
        }

        if (!solve(recurrence, baseCasesPurrs)) {
            debugRecursion("Could not solve recurrence");
            return false;
        }

        RightHandSide res;

        GiNaC::exmap varReSub;
        varReSub.emplace(Purrs::Expr(Purrs::Recurrence::n).toGiNaC(), realVarGiNaC);
        res.term = TT::Expression(recurrence.toGiNaC().subs(varReSub));


        debugRecursion("===Constructing guard===");
        debugRecursion("using guard of recursion:");
        TT::ExpressionVector preEvaluatedGuard;
        for (const TT::Expression &ex : recursion->guard) {
            debugRecursion(ex);
            res.guard.push_back(ex);
            preEvaluatedGuard.push_back(ex);
        }

        // We already have the definition for this function symbol
        // Evaluate all occurences in the guard and the cost
        TT::Expression dummy(GiNaC::numeric(0));
        TT::FunctionDefinition funDef(itrs, funSymbolIndex, res.term, dummy, res.guard);

        debugRecursion("Pre-evaluated guard:");
        for (int i = 0; i < preEvaluatedGuard.size(); ++i) {
            preEvaluatedGuard[i] = preEvaluatedGuard[i].evaluateFunction(funDef, nullptr, nullptr).ginacify();
            debugRecursion(preEvaluatedGuard[i]);
        }
        // Update funDef
        // TODO optimize
        funDef = TT::FunctionDefinition(itrs, funSymbolIndex, res.term, dummy, preEvaluatedGuard);

        debugRecursion("Evaluated guard:");
        for (int i = 0; i < res.guard.size(); ++i) {
            TT::Expression temp = std::move(res.guard[i]);
            res.guard[i] = std::move(temp.evaluateFunction(funDef, nullptr, &res.guard).ginacify());

            debugRecursion(res.guard[i]);
        }

        int oldSize = res.guard.size();
        TT::Expression costRecurrence = recursion->cost.evaluateFunction(funDef, nullptr, &res.guard).ginacify();
        for (int i = oldSize; i < res.guard.size(); ++i) {
            res.guard[i] = res.guard[i].ginacify();
        }
        debugRecursion("After evaluating cost:");
        for (const TT::Expression &ex : res.guard) {
            debugRecursion(ex);
        }


        debugRecursion("===Solving cost===");
        for (const TT::Expression &funApp : recursion->term.getFunctionApplications()) {
            TT::Expression update = funApp.op(realVarIndex);
            std::vector<TT::Expression> updateAsVector;
            updateAsVector.push_back(update);
            costRecurrence = costRecurrence + TT::Expression(funSymbolIndex, itrs.getFunctionSymbolName(funSymbolIndex), updateAsVector);
        }
        recurrence = costRecurrence.substitute(varSub).toPurrs(0);

        baseCasesPurrs.clear();
        for (auto const &pair : baseCases) {
            baseCasesPurrs.emplace(pair.first, pair.second->cost.toPurrs());
        }

        if (!solve(recurrence, baseCasesPurrs)) {
            debugRecursion("Could not solve recurrence");
            return false;
        }

        res.cost = TT::Expression(recurrence.toGiNaC().subs(varReSub));


        debugRecursion("===Resulting rhs===");
        debugRecursion(res);

        result.push_back(std::move(res));

        return true;
}


bool Recursion::findRecursions() {
    debugRecursion("===Finding recursions===");

    for (const RightHandSide *rhs : rightHandSides) {
        std::set<FunctionSymbolIndex> funSymbols = std::move(rhs->term.getFunctionSymbols());

        if (funSymbols.size() == 1 && funSymbols.count(funSymbolIndex) == 1) {
            debugRecursion("Found recursion: " << *rhs);

            // Check if there are any function symbols besides funSymbol in the cost/guard
            funSymbols = std::move(rhs->term.getFunctionSymbols());
            if (funSymbols.size() > 0) {
                if (funSymbols.size() > 1 || funSymbols.count(funSymbolIndex) == 0) {
                    debugRecursion("cost contains an alien function symbol");
                    continue;
                }
            }

            for (const TT::Expression &ex : rhs->guard) {
                funSymbols = std::move(ex.getFunctionSymbols());

                if (funSymbols.size() > 0) {
                    if (funSymbols.size() > 1 || funSymbols.count(funSymbolIndex) == 0) {
                        debugRecursion("guard contains an alien function symbol");
                    }
                }
            }

            if (findRealVars(rhs->term)) {
                debugRecursion("Recursion is suitable");
                recursions.push_back(rhs);
            }
        }
    }

    for (const RightHandSide *rhs : recursions) {
        rightHandSides.erase(rhs);
    }

    return !recursions.empty();
}


bool Recursion::findRealVars(const TT::Expression &term) {
    debugRecursion("===Finding real recursion variables===");
    const std::vector<VariableIndex> &vars = funSymbol.getArguments();

    std::vector<TT::Expression> funApps = std::move(term.getFunctionApplications());
    for (int i = 0; i < vars.size(); ++i) {
        ExprSymbol var = itrs.getGinacSymbol(vars[i]);
        debugRecursion("variable: " << var);

        for (const TT::Expression &funApp : funApps) {
            debugRecursion("function application: " << funApp);
            assert(funApp.nops() == vars.size());

            TT::Expression update = funApp.op(i);
            debugRecursion("update: " << update);
            if (!update.hasNoFunctionSymbols()) {
                debugRecursion("Update contains function symbol, cannot continue");
                return false;
            }

            if (var != update.toGiNaC()) {
                debugRecursion("real");
                realVars.insert(i);
            }
        }
    }

    return realVars.size() >= 1 && realVars.size() <= 2;
}


bool Recursion::findBaseCases() {
    debugRecursion("===Searching for base cases===");

    for (const RightHandSide *rhs : rightHandSides) {
        if (!rhs->term.hasNoFunctionSymbols()) {
            continue;
        }

        std::vector<Expression> query;
        for (const TT::Expression &ex : rhs->guard) {
            assert(ex.hasNoFunctionSymbols());
            query.push_back(ex.toGiNaC());
        }

        Z3VariableContext context;
        z3::model model(context, Z3_model());
        z3::check_result result;
        result = Z3Toolbox::checkExpressionsSAT(query, context, &model);

        debugRecursion("Examining " << *rhs << " as a potential base case");
        if (result == z3::sat) {
            Expression value = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(realVarGiNaC, context));
            if (value.info(GiNaC::info_flags::integer)
                && value.info(GiNaC::info_flags::nonnegative)) {
                // TODO Check range
                Purrs::index_type asUInt = GiNaC::ex_to<GiNaC::numeric>(value).to_int();
                if (baseCases.count(asUInt) == 0) {
                    // TODO Try to derive more base cases from one rhs
                    debugRecursion("is a potential base case for " << realVarGiNaC << " = " << asUInt);
                    baseCases.emplace(asUInt, rhs);
                    continue;

                } else {
                    debugRecursion("Discarding potential base case for " << realVarGiNaC << " = " << asUInt);
                }

            } else {
                debugRecursion("Error, " << value << " is not a natural number");
            }

        } else {
            debugRecursion("Z3 was not sat");
        }
    }

    return !baseCases.empty();
}


bool Recursion::baseCasesAreSufficient() {
    debugRecursion("===Checking if base cases are sufficient===");
    std::vector<TT::Expression> funApps = std::move(recursion->term.getFunctionApplications());

    for (const TT::Expression &funApp : funApps) {
        TT::Expression update = funApp.op(realVarIndex);
        debugRecursion("Update: " << update);
        assert(update.hasNoFunctionSymbols());
        GiNaC::exmap updateSub;
        updateSub.emplace(realVarGiNaC, update.toGiNaC());

        std::vector<std::vector<Expression>> queryRhs; // disjunction of conjunctions
        debugRecursion("RHS:");
        for (auto const &pair : baseCases) {
            debugRecursion("OR (updated base case guard)");
            std::vector<Expression> updatedGuard;
            for (const TT::Expression &ex : pair.second->guard) {
                if (!ex.hasNoFunctionSymbols()) {
                    debugRecursion("Warning: guard contains function symbol, substituting by variable");
                }

                // Using toGiNaC(true) to substitute function symbols by variables
                updatedGuard.push_back(ex.toGiNaC(true).subs(updateSub));
                debugRecursion("\tAND " << updatedGuard.back());
            }
            Expression forceCritVar(realVarGiNaC == pair.first);
            updatedGuard.push_back(forceCritVar.subs(updateSub));
            debugRecursion("\tAND (forcing realVar) " << updatedGuard.back());

            queryRhs.push_back(std::move(updatedGuard));
        }

        std::vector<Expression> queryLhs; // conjunction
        for (const TT::Expression &ex : recursion->guard) {
            if (!ex.hasNoFunctionSymbols()) {
                debugRecursion("Warning: guard contains function symbol, substituting by variable");
            }

            // Using toGiNaC(true) to substitute function symbols by variables

            queryLhs.push_back(ex.toGiNaC(true));
        }

        for (const TT::Expression &negateEx : recursion->guard) {
            debugRecursion("negateEx: " << negateEx);
            // Using toGiNaC(true) to substitute function symbols by variables
            queryLhs.push_back(GT::negate((Expression)negateEx.toGiNaC(true).subs(updateSub)));

            debugRecursion("LHS:");
            for (const Expression &ex : queryLhs) {
                debugRecursion("AND " << ex);
            }

            if (!Z3T::checkTautologicImplication(queryLhs, queryRhs)) {
                debugRecursion("FALSE");
                return false;
            }

            debugRecursion("TRUE");

            queryLhs.pop_back();
        }
    }

    return true;
}


bool Recursion::solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc) {
    debugRecursion("Solving recurrence: " << recurrence);
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(recurrence);
        debugRecursion("base cases:");
        for (auto const &pair : bc) {
            debugRecursion(realVarGiNaC << " = " << pair.first << " is " << pair.second);
        }
        rec.set_initial_conditions(bc);

        auto res = rec.compute_exact_solution();
        if (res != Purrs::Recurrence::SUCCESS) {
        debugRecursion("Purrs failed (not SUCCESS)");
            return false;
        }

        rec.exact_solution(exact);

    } catch (...) {
        debugRecursion("Purrs failed (Exception)");
        return false;
    }

    debugRecursion("solution: " << exact);

    recurrence = exact;
    return true;
}
