#include "recursion.h"

#include <utility>
#include <purrs.hh>

#include "guardtoolbox.h"
#include "recursiongraph.h"
#include "term.h"
#include "z3toolbox.h"

namespace GT = GuardToolbox;
namespace Z3T = Z3Toolbox;

Recursion::Recursion(ITRSProblem &itrs,
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

    if (!findBaseCases()) {
        debugRecursion("No suitable base cases found");
        return false;
    }

    dumpRightHandSides();

    moveRecursiveCallsToGuard();
    dumpRightHandSides();

    instantiateFreeVariables();
    dumpRightHandSides();

    evaluateSpecificRecursiveCalls();
    dumpRightHandSides();

    for (const RightHandSide &rhs : recursion) {
        debugRecursion("Handling " << rhs);
        std::set<int> realVars = std::move(findRealVars(rhs));

        if (realVars.empty()) {
            debugRecursion("Recursion has no real vars!");

        } else {
            for (int mainVarIndex : realVars) {
                solveRecursionWithMainVar(rhs, realVars, mainVarIndex);
            }
        }
    }

    return true;
}


bool Recursion::findRecursions() {
    debugRecursion("===Finding recursions===");

    for (const RightHandSide *rhs : rightHandSides) {
        std::set<FunctionSymbolIndex> funSymbols = std::move(rhs->term.getFunctionSymbols());

        for (const TT::Expression &ex : rhs->guard) {
            std::set<FunctionSymbolIndex> exSymbols = std::move(ex.getFunctionSymbols());
            funSymbols.insert(exSymbols.begin(), exSymbols.end());
        }

        if (funSymbols.size() == 1 && funSymbols.count(funSymbolIndex) == 1) {
            debugRecursion("Found recursion: " << *rhs);

            // Make sure that there are no nested function symbols
            std::vector<TT::Expression> updates = std::move(rhs->term.getUpdates());
            for (const TT::Expression ex : updates) {
                if (ex.hasFunctionSymbol()) {
                    debugRecursion("recursion has nested function symbol");
                    continue;
                }
            }

            for (const TT::Expression &ex : rhs->guard) {
                updates = std::move(rhs->term.getUpdates());
                for (const TT::Expression ex : updates) {
                    if (ex.hasFunctionSymbol()) {
                        debugRecursion("recursion has nested function symbol");
                        continue;
                    }
                }
            }

            debugRecursion("Recursion is suitable");
            recursions.push_back(*rhs);
        }
    }

    return !recursions.empty();
}


bool Recursion::findBaseCases() {
    debugRecursion("===Finding base cases===");

    for (const RightHandSide *rhs : rightHandSides) {
        std::set<FunctionSymbolIndex> funSymbols = std::move(rhs->term.getFunctionSymbols());

        for (const TT::Expression &ex : rhs->guard) {
            std::set<FunctionSymbolIndex> exSymbols = std::move(ex.getFunctionSymbols());
            funSymbols.insert(exSymbols.begin(), exSymbols.end());
        }

        if (funSymbols.empty()) {
            debugRecursion("Found base case: " << *rhs);
            baseCases.push_back(*rhs);
        }
    }

    return !baseCases.empty();
}


void Recursion::moveRecursiveCallsToGuard() {
    debugRecursion("=== Moving all function symbols to the guard ===");
    for (RightHandSide &rhs : recursions) {
        rhs.term = rhs.term.moveFunctionSymbolsToGuard(itrs, rhs.guard);
    }
}


void Recursion::instantiateFreeVariables() {
    debugRecursion("=== Instantiating free variables ===");
    std::vector<int> instantiated;

    for (int i = 0; i < recursions.size(); ++i) {
        // the following call might add new elements to "recursions"
        if (instantiateAFreeVariableOf(recursions[i])) {
            instantiated.push_back(i);
        }
    }

    for (auto it = instantiated.rbegin(); it != instantiated.rend(); ++it) {
        recursions.erase(recursions.begin() + *it);
    }
}


bool Recursion::instantiateAFreeVariableOf(const RightHandSide &rule) {
    ExprSymbolSet freeVars = std::move(getNonChainingFreeVariablesOf(rule));

    if (freeVars.empty()) {
        return false;
    }

    int num = 0;
    ExprSymbol freeVar = *(freeVars.begin());
    TT::Expression freeVarEx = TT::Expression(symbol);
    for (const TT::Expression &ex : recursionCopy.guard) {
        TT::Expression lessEqOrEq;

        if (ex.info(TT::InfoFlag::RelationEqual)) {
            lessEqOrEq = ex;

        } else if (!ex.info(TT::InfoFlag::RelationNotEqual)) {
            lessEqOrEq = GuardToolbox::makeLessEqual(ex);

        } else {
            continue;
        }

        TT::Expression lhs = lessEqOrEq.op(0);
        TT::Expression rhs = lessEqOrEq.op(1);

        if (lhs.equals(freeVarEx) && !rhs.hasFunctionSymbol()) {
            GiNaC::exmap sub;
            sub.emplace(symbol, rhs.toGiNaC());

            recursions.push_back(rule);
            recursions.back().substitute(sub);
            num++;

        } else if (rhs.equals(freeVarEx) && lhs.hasNoFunctionSymbols()) {
            GiNaC::exmap sub;
            sub.emplace(symbol, lhs.toGiNaC());

            recursions.push_back(rule);
            recursions.back().substitute(sub);
            num++;
        }
    }

    if (num == 0) {
        GiNaC::exmap sub;
        sub.emplace(symbol, GiNaC::numeric(1)); // default value

        recursions.push_back(rule);
        recursions.back().substitute(sub);
    }

    return true;
}


void Recursion::evaluateSpecificRecursiveCalls() {
    debugRecursion("=== Trying to evaluate specific recursive calls ===");
    std::vector<int> evaluated;

    for (int i = 0; i < recursions.size(); ++i) {
        // the following call might add new elements to "recursions"
        if (evaluateASpecificRecursiveCallOf(recursions[i])) {
            evaluated.push_back(i);
        }
    }

    for (auto it = evaluated.rbegin(); it != evaluated.rend(); ++it) {
        recursions.erase(recursions.begin() + *it);
    }
}


bool Recursion::evaluateASpecificRecursiveCallOf(const RightHandSide &recursion) {
    bool addedNew = false;

    for (const RightHandSide &bc : baseCases) {
        TT::FunctionDefinition funDef(itrs, funSymbolIndex, bc.term, bc.cost, bc.guard);

        // Note: there are no recursive calls in right-hand sides

        for (int i = 0; i < recursion.guard.size(); ++i) {
            bool modified = false;
            TT::Expression eval;
            TT::Expression addToCost = TT::Expression(GiNaC::numeric(0));
            eval = recursion.guard[i].evaluateFunctionIfLegal(funDef, recursion.guard, &addToCost, modified);
            if (modified) {
                recursions.push_back(recursion);
                RightHandSide &newRhs = recursions.back();
                newRhs.guard[i] = eval;
                newRhs.cost = newRhs.cost + addToCost;
                addedNew = true;

                // Check if a guard substitution is possible now
                TT::Expression &guardI = newRhs.guard[i];
                if (guardI.info(InfoFlag::RelationEqual)) {
                    TT::Expression lhs = guardI.op(0);
                    TT::Expression rhs = guardI.op(1);
                    if (lhs.info(InfoFlag::Variable)
                        && itrs.isChainingVariable(GiNaC::ex_to<GiNaC::sym>(lhs.toGiNaC()))
                        && !rhs.hasFunctionSymbol()) {
                        GiNaC::exmap guardSub;
                        guardSub.emplace(GiNaC::ex_to<GiNaC::sym>(lhs.toGiNaC()),
                                         rhs.toGiNaC());
                        newRhs.guard.erase(newRhs.guard.begin() + i);
                        newRhs.substitute(guardSub);
                    }
                }
            }
        }

    }

    return addedNew;
}


std::set<int> Recursion::findRealVars(const RightHandSide &rhs) {
    debugRecursion("===Finding real recursion variables===");
    std::set<int> realVars;
    const std::vector<VariableIndex> &vars = funSymbol.getArguments();

    std::vector<TT::Expression> funApps;
    assert(!rhs.term.hasFunctionSymbol());
    for (const TT::Expression &ex : rhs.guard) {
        ex.collectFunctionApplications(funApps);
    }

    for (int i = 0; i < vars.size(); ++i) {
        ExprSymbol var = itrs.getGinacSymbol(vars[i]);
        debugRecursion("variable: " << var);

        for (const TT::Expression &funApp : funApps) {
            debugRecursion("function application: " << funApp);
            assert(funApp.nops() == vars.size());

            TT::Expression update = funApp.op(i);
            debugRecursion("update: " << update);
            assert(update.hasNoFunctionSymbols());

            if (var != update.toGiNaC()) {
                debugRecursion("real");
                realVars.insert(i);
            }
        }
    }

    return realVars;
}


void Recursion::solveRecursionWithMainVar(const RightHandSide &recursion, const std::set<int> &realVars, int mainVarIndex) {
    debugRecursion("=== Trying to solve recursion ===");
    debugRecursion("mainVarIndex: " << mainVarIndex);
    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);

    // search for base cases
    BaseCaseIndexMap baseCaseIndexMap = analyzeBaseCases(mainVar);
    if (baseCaseIndexMap.empty()) {
        debugRecursion("Found no usable base cases");
        return;
    }

    BaseCaseRhsMap baseCaseMap;
    for (const auto &pair : baseCaseIndexMap) {
        Purrs::index_type val = pair.first;
        baseCaseMap.emplace(val, baseCases[pair.second]);

        GiNaC::exmap sub;
        sub.emplace(mainVarGiNaC, val);
        baseCaseMap[val].substitute(sub);
        baseCaseMap[val].guard.push_back(mainVarGiNaC == GiNaC::numeric(val));
    }

    // separate the guard
    TT::Expression normalGuard;
    RecursiveCallMap recCallMap;
    TT::ExpressionVector srGuard;
    for (const TT::Expression &ex : recursion.guard) {
        ExprSymbolSet chainVars = std::move(getChainingVariablesOf(ex));
        if (ex.empty()) { // normal guard
            normalGuard.push_back(ex);

        } else if (ex.hasFunctionSymbol()) { // recursive call
            assert(ex.info(InfoFlag::RelationEqual));
            TT::Expression lhs = ex.op(0);
            TT::Expression rhs = ex.op(1);
            assert(ex.info(InfoFlag::Variable));
            ExprSymbol var = GiNaC::ex_to<GiNaC::symbol>(lhs.toGiNaC());
            recCallMap.emplace(var, rhs);

        } else { // self-referential guard
            srGuard.push_back(ex);
        }
    }

    // collect all updates
    std::vector<TT::Expression> recCalls;
    for (const auto &pair : recCallMap) {
        pair.second.collectFunctionApplications(recCalls);
    }
    std::vector<GiNaC::exmap> updates;
    for (const TT::Expression &recCall : recCalls) {
        GiNaC::exmap update;
        assert(recCall.info(InfoFlag::FunctionSymbol));
        assert(recCall.nops() == funSymbol.getArguments().size());

        for (int i = 0; i < recCall.nops(); ++i) {
            VariableIndex varIndex = funSymbol.getArguments()[i];
            ExprSymbol var = itrs.getGinacSymbol(varIndex);
            TT::Expression update = recCall.op(i);
            assert(update.hasNoFunctionSymbols());

            update.emplace(var, update.toGiNaC());
        }
        updates.push_back(std::move(update));
    }

    // check if the base cases match
    if (!baseCasesMatch(baseCaseMap, updates, normalGuard)) {
        return;
    }

    VariableConfig config = std::move(generateMultiVarConfig(updates, realVars, mainVarIndex));
}


BaseCaseIndexMap Recursion::analyzeBaseCases(ExprSymbol mainVar) {
    debugRecursion("=== analyzing base cases ===");
    debugRecursion("mainVar: " << mainVar);
    BaseCaseIndexMap map;

    for (int i = 0; i < baseCases.size(); ++i) {
        const RightHandSide &bc = baseCases[i];
        std::vector<Expression> query;
        for (const TT::Expression &ex : rhs->guard) {
            assert(ex.hasNoFunctionSymbols());
            query.push_back(ex.toGiNaC());
        }

        Z3VariableContext context;
        z3::model model(context, Z3_model());
        z3::check_result result;
        result = Z3Toolbox::checkExpressionsSAT(query, context, &model);

        debugRecursion("analyzing " << bc << " as a potential base case");
        if (result == z3::sat) {
            Expression value = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(mainVar, context));
            if (value.info(GiNaC::info_flags::integer)
                && value.info(GiNaC::info_flags::nonnegative)) {
                // Check range?
                Purrs::index_type asUInt = GiNaC::ex_to<GiNaC::numeric>(value).to_int();
                if (map.count(asUInt) == 0) {
                    // Try to derive more base cases from one rhs?
                    debugRecursion("is a potential base case for " << mainVar << " = " << asUInt);
                    map.emplace(asUInt, i);
                    continue;

                } else {
                    debugRecursion("Discarding potential base case for " << mainVar << " = " << asUInt);
                }

            } else {
                debugRecursion("Error, " << value << " is not a natural number");
            }

        } else {
            debugRecursion("Z3 was not sat");
        }
    }

    return map;
}


bool Recursion::baseCasesMatch(const BaseCaseRhsMap &bcs, const std::vector<GiNaC::exmap> &updates, const TT::Expression &normalGuard) {
    debugRecursion("=== Checking if base cases match ===");
    debugRecursion("Base Cases:");
    for (const auto &pair : bcs) {
        debugRecursion(pair.first << ": " << pair.second);
    }

    debugRecursion("Recursive Calls:");
    for (const TT::Expression &funApp : recCalls) {
        debugRecursion(funApp);
    }

    debugRecursion("Guard:");
    for (const TT::Expression &ex : normalGuard) {
        debugRecursion(ex);
    }


    // check if the base cases match
    for (const GiNaC::exmap &update : updates) {
        debugRecursion("Now handling update: " << update);

        std::vector<std::vector<Expression>> queryRhs;
        // disjunction of conjunctions (updated base cases)
        debugRecursion("RHS:");
        for (auto const &pair : bcs) {
            debugRecursion("OR (updated base case guard)");
            std::vector<Expression> updatedGuard;
            for (const TT::Expression &ex : pair.second->guard) {
                assert(!ex.hasFunctionSymbol());

                updatedGuard.push_back(ex.toGiNaC().subs(update));
                debugRecursion("\tAND " << updatedGuard.back());
            }

            queryRhs.push_back(std::move(updatedGuard));
        }

        std::vector<Expression> queryLhs;
        // conjunction (guard and not updated guard)
        for (const TT::Expression &ex : normalGuard) {
            assert(!ex.hasFunctionSymbol());
            queryLhs.push_back(ex.toGiNaC());
        }

        // note: we split the left-hand side into multiple left-hand sides
        // (one negation each)
        for (const TT::Expression &negateEx : normalGuard) {
            debugRecursion("negating: " << negateEx);
            Expression toNegate = negateEx.toGiNaC().subs(update);
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


VariableConfig generateMultiVarConfig(const std::vector<GiNaC::exmap> &updates, const std::set<int> &realVars, int mainVarIndex) {
    VariableConfig config;

    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);

    const std::vector<VariableIndex> &variables = funSymbol.getArguments();
    for (int i = 0; i < variables.size(); ++i) {
        if (i == mainVarIndex) {
            continue;
        }

        VariableIndex var = variables[i];
        ExprSymbol varGiNaC = itrs.getGinacSymbol(var);

        if (realVars.count(i) == 0) {
            debugRecursion("arg " << mainVarGiNaC << " -> " << varGiNaC << ": not a real var");

        } else if (updatesHaveConstDifference(updates, mainVarIndex, i)) {
            debugRecursion("arg " << mainVarGiNaC << " -> " << varGiNaC << ": constant difference");
            ExprSymbol cdiff("cdiff" + std::to_string(i));
            config.sub.emplace(varGiNaC, mainVarGiNaC - cdiff);
            config.reSub.emplace(cdiff, mainVarGiNaC - varGiNaC);

        } else if (updatesHaveConstSum(updates, mainVarIndex, i)) {
            debugRecursion("arg " << mainVarGiNaC << " -> " << varGiNaC << ": constant sum");
            ExprSymbol csum("csum" + std::to_string(i));
            config.sub.emplace(varGiNaC, csum - mainVarGiNaC);
            config.reSub.emplace(csum, mainVarGiNaC + varGiNaC);

        } else {
            config.error = true;
        }
    }

    return config;
}


bool Recursion::updatesHaveConstDifference(const std::vector<GiNaC::exmap> &updates, int mainVarIndex, int varIndex) const {
    debugRecursion("=== Checking if updates have constant difference ===");
    debugRecursion("main var index: " << mainVarIndex);
    debugRecursion("checking var index: " << varIndex);

    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);
    VariableIndex var = funSymbol.getArguments()[varIndex];
    ExprSymbol varGiNaC = itrs.getGinacSymbol(var);

    for (const GiNaC::exmap &update : update) {
        assert(update.count(mainVarGiNaC) > 0);
        assert(update.count(varGiNaC) > 0);

        TT::Expression updateMainVar = std::move(funApp.op(mainVarIndex));
        TT::Expression updateVar = std::move(funApp.op(varIndex));
        debugRecursion("Update (main var): " << updateMainVar << ", update: " << updateVar);
        assert(!updateMainVar.hasFunctionSymbol());
        assert(!updateVar.hasFunctionSymbol());

        Expression updateMainVarGiNaC = updateMainVar.toGiNaC();
        Expression updateVarGiNaC = updateVar.toGiNaC();

        // e.g. (x - 1) - (y - 1) = x - y
        GiNaC::ex diff = updateMainVarGiNaC - updateVarGiNaC;
        // e.g. x - y
        GiNaC::ex varDiff = mainVarGiNaC - varGiNaC;

        if (!diff.is_equal(varDiff)) {
            return false;
        }

        // x - 1 == (y - 1)[y/x]
        GiNaC::exmap sub;
        sub.emplace(varGiNaC, mainVarGiNaC);
        if (!updateMainVarGiNaC.is_equal(updateVarGiNaC.subs(sub))) {
            return false;
        }
    }

    return true;
}


bool Recursion::updatesHaveConstSum(const std::vector<GiNaC::exmap> &updates, int mainVarIndex, int varIndex) const {
    debugRecursion("=== Checking if updates have constant difference ===");
    debugRecursion("main var index: " << mainVarIndex);
    debugRecursion("checking var index: " << varIndex);

    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);
    VariableIndex var = funSymbol.getArguments()[varIndex];
    ExprSymbol varGiNaC = itrs.getGinacSymbol(var);

    for (const GiNaC::exmap &update : update) {
        assert(update.count(mainVarGiNaC) > 0);
        assert(update.count(varGiNaC) > 0);

        TT::Expression updateMainVar = std::move(funApp.op(mainVarIndex));
        TT::Expression updateVar = std::move(funApp.op(varIndex));
        debugRecursion("Update (main var): " << updateMainVar << ", update: " << updateVar);
        assert(!updateMainVar.hasFunctionSymbol());
        assert(!updateVar.hasFunctionSymbol());

        Expression updateMainVarGiNaC = updateMainVar.toGiNaC();
        Expression updateVarGiNaC = updateVar.toGiNaC();

        // e.g. (x - 1) + (y + 1) = x + y
        GiNaC::ex sum = updateMainVarGiNaC + updateVarGiNaC;
        // e.g. x + y
        GiNaC::ex varSum = mainVarGiNaC + varGiNaC;

        if (!sum.is_equal(varSum)) {
            return false;
        }

        // x - 1 - ((y + 1)[y/x]) is a number
        GiNaC::exmap sub;
        sub.emplace(varGiNaC, mainVarGiNaC);
        GiNaC::ex ex = updateMainVarGiNaC - updateVarGiNaC.subs(sub);
        if (!GiNaC::is_a<GiNaC::numeric>(ex)) {
            return false;
        }
    }

    return true;
}


bool Recursion::solveRecursionInOneVar() {
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
        debugRecursion("using (non-self-referential) guard of recursion:");
        for (const TT::Expression &ex : rcGuard) {
            TT::Expression freeVarEx = std::move(ex.substitute(freeVarSub));
            debugRecursion(freeVarEx);
            res.guard.push_back(freeVarEx);
        }

        // We already have the definition for this function symbol
        // Evaluate all occurences in the cost
        TT::Expression dummy(GiNaC::numeric(0));
        TT::ExpressionVector dummyVector;
        TT::FunctionDefinition funDef(itrs, funSymbolIndex, res.term, dummy, dummyVector);
        TT::Expression costRecurrence = recursionCopy.cost.evaluateFunction(funDef, nullptr, nullptr).ginacify();

        debugRecursion("===Solving cost===");
        for (const TT::Expression &funApp : recursionCopy.term.getFunctionApplications()) {
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
            debugRecursion("Could not solve cost recurrence");
            return false;
        }

        res.cost = TT::Expression(recurrence.toGiNaC().subs(varReSub));


        // Make sure that removing the self-references from the guard is sound
        if (!selfReferentialGuard.empty() && !removingSelfReferentialGuardIsSound(selfReferentialGuard)) {
            debugRecursion("removing self-referential parts of the guard is possibly not sound, aborting");
            return false;
        }


        // Add all instantiations to the guard
        for (auto const &pair : instSub) {
            res.guard.push_back(TT::Expression(pair.first == pair.second));
        }

        // Use heuristic to merge guards
        mergeBaseCaseGuards(res.guard);

        debugRecursion("===Resulting rhs===");
        debugRecursion(res);

        result.push_back(std::move(res));

        return true;
}


bool Recursion::solveRecursionInTwoVars() {
    debugRecursion("===Trying to solve recursion in 2 vars===");

    // TODO: const sum
    // check for constant difference, e.g., f(x,y) = f(x-1,y-1)
    if (updatesHaveConstDifference(recursion->term)) {
        debugRecursion("constant difference");

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

    } else if (updatesHaveConstSum(recursion->term)) {
        debugRecursion("constant sum");

        // rewrite the recursion
        realVarSub.clear();
        realVarSub.emplace(realVar2GiNaC, constDiff - realVarGiNaC);

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
            constDiffSub.emplace(constDiff, realVarGiNaC + realVar2GiNaC);

            RightHandSide &resRhs = result.back();
            resRhs.term = resRhs.term.substitute(constDiffSub);
            resRhs.cost = resRhs.cost.substitute(constDiffSub);
            for (TT::Expression &ex : resRhs.guard) {
                ex = ex.substitute(constDiffSub);
            }

            return true;
        }

    } else {
        debugRecursion("neither constant difference nor constant sum");
    }

    return false;
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


bool Recursion::removingSelfReferentialGuardIsSound(const TT::ExpressionVector &srGuard) const {
    debugRecursion("===Checking if removing the self-referential parts of the guard is sound===");
    TT::Expression dummy(GiNaC::numeric(0));
    TT::ExpressionVector dummyVector;

    // IB: For every base case: selfReferentialGuard[funSymbol/baseCase] holds
    for (auto const &pair : baseCases) {
        TT::FunctionDefinition funDef(itrs, funSymbolIndex, pair.second->term, dummy, dummyVector);

        std::vector<Expression> query;
        for (const TT::Expression &ex : srGuard) {
            TT::Expression query = std::move(ex.substitute(instSub).evaluateFunction(funDef, nullptr, nullptr));
            GiNaC::ex asGiNaC = std::move(query.toGiNaC(true));

            if (!Z3Toolbox::checkTautology(asGiNaC)) {
                debugRecursion("query: " << asGiNaC << ": FALSE");
                debugRecursion("Potentially not sound");
                return false;

            }
            debugRecursion("query: " << asGiNaC << ": TRUE");
        }
    }

    // IS: For the recursion: guard && srGuard => recursion.term
    TT::FunToVarSub sub; // make sure that identical function calls are substituted by the same variable
    std::vector<Expression> lhs;
    for (const TT::Expression &ex : recursionCopy.guard) {
        lhs.push_back(ex.toGiNaC(true, nullptr, &sub));
    }
    for (const TT::Expression &ex : srGuard) {
        lhs.push_back(ex.toGiNaC(true, nullptr, &sub));
    }

    debugRecursion("LHS:");
    for (const Expression &ex : lhs) {
        debugRecursion(ex);
    }

    for (const TT::Expression &ex : srGuard) {
        Expression rhs = ex.substitute(funSymbolIndex, recursionCopy.term).toGiNaC(true, nullptr, &sub);

        if (!Z3Toolbox::checkTautologicImplication(lhs, rhs)) {
            debugRecursion("query: LHS => " << rhs << ": FALSE");
            debugRecursion("Potentially not sound");
            return false;
        }
        debugRecursion("query: LHS => " << rhs << ": TRUE");
    }

    debugRecursion("It is sound to remove the following parts of the guard:");
    for (const TT::Expression &ex : srGuard) {
        debugRecursion(ex);
    }

    return true;
}

void Recursion::mergeBaseCaseGuards(TT::ExpressionVector &recGuard) const {
    debugRecursion("===Merging base case guards===");

    for (auto const &pair : baseCases) {
        const RightHandSide &rhs = *pair.second;

        if (rhs.guard.size() == 1) {
            TT::Expression bcGuard = rhs.guard[0].substitute(realVarSub);

            if (bcGuard.info(TT::InfoFlag::RelationEqual)) {
                TT::Expression bcLhs = bcGuard.op(0).ginacify();
                TT::Expression bcRhs = bcGuard.op(1).ginacify();

                for (TT::Expression &ex : recGuard) {
                    assert(ex.info(TT::InfoFlag::Relation));
                    TT::Expression lhs = ex.op(0).ginacify();
                    TT::Expression rhs = ex.op(1).ginacify();

                    if (ex.info(TT::InfoFlag::RelationGreater)
                        && lhs.equals(bcLhs)
                        && rhs.equals(bcRhs)) { // e.g., A == 0 and A > 0
                        ex = lhs >= rhs;

                    } else if (ex.info(TT::InfoFlag::RelationGreaterEqual)
                               && lhs.equals(bcLhs)
                               && rhs.equals((bcRhs + 1).ginacify())) { // e.g., A == 0 and A >= 1
                        ex = lhs >= bcRhs;
                    }
                }
            }
        }
    }
}


ExprSymbolSet Recursion::getNonChainingFreeVariablesOf(const RightHandSide &rule) {
    ExprSymbolSet freeVars;
    rule.term.collectVariables(freeVars);
    rule.cost.collectVariables(freeVars);
    for (const TT::Expression &ex : rule.guard) {
        ex.collectVariables(freeVars);
    }

    // delete all non-free variables and all chaining variables
    auto it = freeVars.begin();
    while (it != freeVars.end()) {
        if (itrs.isFreeVariable(*it) && !(itrs.isChainingVariable())) {
            ++it;

        } else {
            it = freeVars.erase(it);
        }
    }

    return freeVars;
}


ExprSymbolSet Recursion::getChainingVariablesOf(const RightHandSide &rule) {
    ExprSymbolSet chainVars;
    rule.term.collectVariables(chainVars);
    rule.cost.collectVariables(chainVars);
    for (const TT::Expression &ex : rule.guard) {
        ex.collectVariables(chainVars);
    }

    // delete all non-chaining variables
    auto it = chainVars.begin();
    while (it != chainVars.end()) {
        if (itrs.isChainingVariable()) {
            ++it;

        } else {
            it = chainVars.erase(it);
        }
    }

    return chainVars;
}


void Recursion::dumpRightHandSides() const {
    debugRecursion("Recursions:");
    for (const RightHandSide &rhs: recursions) {
        debugRecursion(rhs);
    }
    debugRecursion("Base Cases:");
    for (const RightHandSide &rhs: baseCases) {
        debugRecursion(rhs);
    }
}