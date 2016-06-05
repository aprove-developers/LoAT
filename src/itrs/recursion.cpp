#include "recursion.h"

#include <utility>
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
      funSymbol(itrs.getFunctionSymbol(funSymbolIndex)),
      constDiff("constDiff") {
}

bool Recursion::solve() {
    assert(wereUsed.empty());
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

        for (const RightHandSide *rhs : recursions) {
            recursion = rhs;
            recursionCopy = *rhs;

            if (solveRecursionInOneVar()) {
                wereUsed.insert(rhs);
            }
        }

    } else {
        realVarIndex = *realVars.begin();
        realVar = funSymbol.getArguments()[realVarIndex];
        realVarGiNaC = itrs.getGinacSymbol(realVar);

        realVar2Index = *(++realVars.begin());
        realVar2 = funSymbol.getArguments()[realVar2Index];
        realVar2GiNaC = itrs.getGinacSymbol(realVar2);

        for (const RightHandSide *rhs : recursions) {
            recursion = rhs;

            // solveRecursionInTwoVars makes the copy
            if (solveRecursionInTwoVars()) {
                wereUsed.insert(rhs);
            }
        }

        std::swap(realVarIndex, realVar2Index);
        std::swap(realVar, realVar2);
        std::swap(realVarGiNaC, realVar2GiNaC);

        for (const RightHandSide *rhs : recursions) {
            recursion = rhs;

            // solveRecursionInTwoVars makes the copy
            if (solveRecursionInTwoVars()) {
                wereUsed.insert(rhs);
            }
        }
    }

    if (!wereUsed.empty()) {
        for (auto const &pair : baseCases) {
            wereUsed.insert(pair.second);
        }
    }

    return !wereUsed.empty();
}


bool Recursion::solveRecursionInOneVar() {
        debugRecursion("===Trying to solve recursion===");
        debugRecursion("Recursion: " << *recursion);

        // try to instantiate variables if the base cases are not sufficient
        instCandidates.clear();
        for (TT::Expression &ex : recursionCopy.guard) {
            ex.substitute(realVarSub).collectVariables(instCandidates);
        }
        instCandidates.erase(realVarGiNaC);
        instCandidates.erase(constDiff);

        instSub.clear();
        bool sufficient;
        while (!(sufficient = baseCasesAreSufficient()) && !instCandidates.empty()) {
            instantiateACandidate();
        }

        if (!sufficient) {
            debugRecursion("Base cases are not sufficient");
            return false;
        }

        debugRecursion("===Solving recursion===");
        GiNaC::exmap varSub;
        varSub.emplace(realVarGiNaC, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
        Purrs::Expr recurrence = recursionCopy.term.substitute(varSub).toPurrs(realVarIndex);

        std::map<Purrs::index_type,Purrs::Expr> baseCasesPurrs;
        for (auto const &pair : baseCases) {
            GiNaC::exmap instantiationSub;
            instantiationSub.emplace(realVarGiNaC, GiNaC::numeric(pair.first));

            TT::Expression res = pair.second->term.substitute(realVarSub).substitute(instantiationSub);
            baseCasesPurrs.emplace(pair.first, res.toPurrs());
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
            TT::Expression exSub = ex.substitute(instSub);
            debugRecursion(exSub);
            res.guard.push_back(exSub);
            preEvaluatedGuard.push_back(exSub);
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
        TT::Expression costRecurrence = recursionCopy.cost.evaluateFunction(funDef, nullptr, &res.guard).ginacify();
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
            GiNaC::exmap instantiationSub;
            instantiationSub.emplace(realVarGiNaC, GiNaC::numeric(pair.first));

            TT::Expression res = pair.second->cost.substitute(realVarSub).substitute(instantiationSub);
            baseCasesPurrs.emplace(pair.first, res.toPurrs());
        }

        if (!solve(recurrence, baseCasesPurrs)) {
            debugRecursion("Could not solve recurrence");
            return false;
        }

        res.cost = TT::Expression(recurrence.toGiNaC().subs(varReSub));

        // Add all instantiations to the guard
        for (auto const &pair : instSub) {
            res.guard.push_back(TT::Expression(pair.first == pair.second));
        }

        debugRecursion("===Resulting rhs===");
        debugRecursion(res);

        result.push_back(std::move(res));

        return true;
}


bool Recursion::solveRecursionInTwoVars() {
    debugRecursion("===Trying to solve recursion in 2 vars===");

    // TODO: const sum
    // check for constant difference, e.g., f(x,y) = f(x-1,y-1)
    if (!updatesHaveConstDifference(recursion->term)) {
        debugRecursion("constDiff fail: " << *recursion);
        return false;
    }

    // rewrite the recursion
    realVarSub.clear();
    realVarSub.emplace(realVar2GiNaC, realVarGiNaC - constDiff);

    recursionCopy.term = recursion->term.substitute(realVarSub);
    recursionCopy.cost = recursion->cost.substitute(realVarSub);
    recursionCopy.guard.clear();
    for (const TT::Expression &ex : recursion->guard) {
        recursionCopy.guard.push_back(ex.substitute(realVarSub));
    }

    if (!findBaseCases()) {
        debugRecursion("Found no usable base cases [2 real variables]");
        return false;
    }

    if (solveRecursionInOneVar()) {
        // Substitute constDiff
        GiNaC::exmap constDiffSub;
        constDiffSub.emplace(constDiff, realVarGiNaC - realVar2GiNaC);

        RightHandSide &resRhs = result.back();
        resRhs.term = resRhs.term.substitute(constDiffSub);
        resRhs.cost = resRhs.cost.substitute(constDiffSub);
        for (TT::Expression &ex : resRhs.guard) {
            ex = ex.substitute(constDiffSub);
        }

        return true;
    }

    return false;
}


void Recursion::instantiateACandidate() {
    debugRecursion("===Instantiating a variable===");
    assert(!instCandidates.empty());
    auto it = instCandidates.begin();
    ExprSymbol toInst = *it;
    instCandidates.erase(it);

    std::vector<Expression> query;
    for (const TT::Expression &ex : recursionCopy.guard) {
        if (ex.hasNoFunctionSymbols()) {
            query.push_back(ex.toGiNaC().subs(realVarSub));
        }
    }

    Z3VariableContext context;
    z3::model model(context, Z3_model());
    z3::check_result result;
    result = Z3Toolbox::checkExpressionsSAT(query, context, &model);

    if (result == z3::sat) {
        Expression value = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(realVarGiNaC, context));
        instSub.emplace(toInst, value);
        debugRecursion(toInst << " -> " << value);

    } else {
        instSub.emplace(toInst, GiNaC::numeric(0));
        debugRecursion(toInst << " -> " << 0);
    }


    recursionCopy.term = recursionCopy.term.substitute(instSub);
    recursionCopy.cost = recursionCopy.cost.substitute(instSub);
    for (TT::Expression &ex : recursionCopy.guard) {
        ex = ex.substitute(instSub);
    }
}


bool Recursion::updatesHaveConstDifference(const TT::Expression &term) const {
    debugRecursion("===Checking if updates have constant difference===");
    std::vector<TT::Expression> funApps = std::move(term.getFunctionApplications());

    for (const TT::Expression &funApp : funApps) {
        TT::Expression update = std::move(funApp.op(realVarIndex));
        TT::Expression update2 = std::move(funApp.op(realVar2Index));
        debugRecursion("Update: " << update << ", update 2: " << update2);
        assert(update.hasNoFunctionSymbols());
        assert(update2.hasNoFunctionSymbols());
        GiNaC::ex updateGiNaC = update.toGiNaC();
        GiNaC::ex update2GiNaC = update2.toGiNaC();

        // e.g. (x - 1) - (y - 1) = x - y
        GiNaC::ex diff = updateGiNaC - update2GiNaC;
        // e.g. x - y
        GiNaC::ex varDiff = realVarGiNaC - realVar2GiNaC;

        if (!diff.is_equal(varDiff)) {
            return false;
        }

        // x - 1 == (y - 1)[y/x]
        GiNaC::exmap sub;
        sub.emplace(realVar2GiNaC, realVarGiNaC);
        if (!updateGiNaC.is_equal(update2GiNaC.subs(sub))) {
            return false;
        }
    }

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

    baseCases.clear();
    for (const RightHandSide *rhs : rightHandSides) {
        if (!rhs->term.hasNoFunctionSymbols()) {
            continue;
        }

        std::vector<Expression> query;
        for (const TT::Expression &ex : rhs->guard) {
            assert(ex.hasNoFunctionSymbols());
            query.push_back(ex.toGiNaC().subs(realVarSub));
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
                updatedGuard.push_back(ex.toGiNaC(true).subs(realVarSub).subs(updateSub));
                debugRecursion("\tAND " << updatedGuard.back());
            }
            Expression forceCritVar(realVarGiNaC == pair.first);
            updatedGuard.push_back(forceCritVar.subs(updateSub));
            debugRecursion("\tAND (forcing realVar) " << updatedGuard.back());

            queryRhs.push_back(std::move(updatedGuard));
        }

        std::vector<Expression> queryLhs; // conjunction
        for (const TT::Expression &ex : recursionCopy.guard) {
            if (!ex.hasNoFunctionSymbols()) {
                debugRecursion("Warning: guard contains function symbol, substituting by variable");
            }

            // Using toGiNaC(true) to substitute function symbols by variables

            queryLhs.push_back(ex.toGiNaC(true));
        }

        for (const TT::Expression &negateEx : recursionCopy.guard) {
            debugRecursion("negateEx: " << negateEx);
            // Using toGiNaC(true) to substitute function symbols by variables
            Expression toNegate = negateEx.toGiNaC(true).subs(updateSub);
            debugRecursion("toNegate: " << toNegate);
            queryLhs.push_back(GT::negate(toNegate));

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
