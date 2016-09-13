#include "recursion.h"

#include <utility>
#include <sstream>
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
      funSymbol(itrs.getFunctionSymbol(funSymbolIndex)) {
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

    for (const ORightHandSide &rhs : recursions) {
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

    for (const ORightHandSide &rhs : closedForms) {
        wereUsed.insert(rhs.origins.cbegin(), rhs.origins.cend());
        result.push_back(rhs);
    }

    return !closedForms.empty();
}


void Recursion::solveRecursionWithMainVar(const ORightHandSide &recursion, const std::set<int> &realVars, int mainVarIndex) {
    debugRecursion("=== Trying to solve recursion ===");
    debugRecursion("mainVarIndex: " << mainVarIndex);
    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);

    // search for base cases
    BaseCaseIndexMap baseCaseIndexMap = analyzeBaseCases(mainVarGiNaC);
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
        TT::Expression forceVar(mainVarGiNaC == GiNaC::numeric(val));
        baseCaseMap[val].guard.push_back(forceVar);
    }

    // separate the guard
    TT::ExpressionVector normalGuard;
    RecursiveCallMap recCallMap;
    TT::ExpressionVector srGuard;
    for (const TT::Expression &ex : recursion.guard) {
        ExprSymbolSet chainVars = std::move(getChainingVariablesOf(ex));
        if (chainVars.empty()) { // normal guard
            normalGuard.push_back(ex);
            assert(!ex.hasFunctionSymbol());

        } else if (ex.hasFunctionSymbol()) { // recursive call
            assert(ex.info(TT::InfoFlag::RelationEqual));
            TT::Expression lhs = ex.op(0);
            TT::Expression rhs = ex.op(1);
            assert(lhs.info(TT::InfoFlag::Variable));
            ExprSymbol var = GiNaC::ex_to<GiNaC::symbol>(lhs.toGiNaC());
            recCallMap.emplace(var, rhs);

        } else { // self-referential guard
            srGuard.push_back(ex);
        }
    }
    debugRecursion("Separated the guard:");
    std::ostringstream debugOut;
    for (const TT::Expression &ex : normalGuard) {
        debugOut << ex << ", ";
    }
    debugOut << std::endl;
    for (const auto &pair : recCallMap) {
        debugOut << pair.first << " == " << pair.second << ", ";
    }
    debugOut << std::endl;
    for (const TT::Expression &ex : srGuard) {
        debugOut << ex << ", ";
    }
    debugRecursion(debugOut.str());

    // collect all updates
    std::vector<TT::Expression> recCalls;
    for (const auto &pair : recCallMap) {
        pair.second.collectFunctionApplications(recCalls);
    }
    UpdateVector updates;
    for (const TT::Expression &recCall : recCalls) {
        GiNaC::exmap update;
        assert(recCall.info(TT::InfoFlag::FunctionSymbol));
        assert(recCall.nops() == funSymbol.getArguments().size());

        for (int i = 0; i < recCall.nops(); ++i) {
            VariableIndex varIndex = funSymbol.getArguments()[i];
            ExprSymbol var = itrs.getGinacSymbol(varIndex);
            TT::Expression ex = recCall.op(i);
            assert(ex.hasNoFunctionSymbols());

            update.emplace(var, ex.toGiNaC());
        }
        updates.push_back(std::move(update));
    }

    // check if the base cases match
    if (!baseCasesMatch(baseCaseMap, updates, normalGuard)) {
        debugRecursion("The base cases do NOT match!");
        return;
    }
    debugRecursion("The base cases match!");

    // check if the self-referential guard is inductively valid
    if (!selfReferentialGuardIsInductivelyValid(normalGuard, recCallMap, baseCaseMap, srGuard)) {
        debugRecursion("The sr-guard is NOT inductively valid!");
        return;
    }
    debugRecursion("The self-referential guard is inductively valid!");

    VariableConfig config = std::move(generateMultiVarConfig(updates, realVars, mainVarIndex));
    if (config.error) {
        return;
    }

    TT::Expression closedRHSs;
    if (!computeClosedFormOfTheRHSs(closedRHSs, recursion, mainVarIndex, config, recCallMap, baseCaseMap)) {
        debugRecursion("Failed to compute a closed form of the RHSs");
    }
    debugRecursion("Closed form of the RHSs: " << closedRHSs);

    TT::Expression closedCosts;
    if (!computeClosedFormOfTheCosts(closedCosts, closedRHSs, recursion, mainVarIndex, config, recCallMap, recCalls, baseCaseMap)) {
        debugRecursion("Failed to compute a closed form of the RHSs");
    }
    debugRecursion("Closed form of the costs: " << closedCosts);

    TT::ExpressionVector guard = std::move(constructGuard(normalGuard, baseCaseMap));

    // Construct closed-form rule
    ORightHandSide closedForm;
    closedForm.term = std::move(closedRHSs);
    closedForm.cost = std::move(closedCosts);
    closedForm.guard = std::move(guard);
    closedForm.origins.insert(recursion.origins.cbegin(),
                              recursion.origins.cend());
    for (auto const &pair : baseCaseMap) {
        closedForm.origins.insert(pair.second.origins.cbegin(),
                                  pair.second.origins.cend());
    }

    debugRecursion("Constructed closed-form rule:");
    debugRecursion(closedForm);
    closedForms.push_back(std::move(closedForm));
}


bool Recursion::findRecursions() {
    debugRecursion("=== Finding recursions ===");

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
            recursions.back().origins.insert(rhs);
        }
    }

    return !recursions.empty();
}


bool Recursion::findBaseCases() {
    debugRecursion("=== Finding base cases ===");

    for (const RightHandSide *rhs : rightHandSides) {
        std::set<FunctionSymbolIndex> funSymbols = std::move(rhs->term.getFunctionSymbols());

        for (const TT::Expression &ex : rhs->guard) {
            std::set<FunctionSymbolIndex> exSymbols = std::move(ex.getFunctionSymbols());
            funSymbols.insert(exSymbols.begin(), exSymbols.end());
        }

        if (funSymbols.empty()) {
            debugRecursion("Found base case: " << *rhs);
            baseCases.push_back(*rhs);
            baseCases.back().origins.insert(rhs);
        }
    }

    return !baseCases.empty();
}


void Recursion::moveRecursiveCallsToGuard() {
    debugRecursion("=== Moving all function symbols to the guard ===");
    for (ORightHandSide &rhs : recursions) {
        rhs.term = rhs.term.moveFunctionSymbolsToGuard(itrs, rhs.guard);
    }
}


void Recursion::instantiateFreeVariables() {
    debugRecursion("=== Instantiating free variables ===");
    std::vector<int> instantiated;

    for (int i = 0; i < recursions.size(); ++i) {
        // the following call might add new elements to "recursions"
        ORightHandSide copy = recursions[i];
        if (instantiateAFreeVariableOf(copy)) {
            instantiated.push_back(i);
        }
    }

    for (auto it = instantiated.rbegin(); it != instantiated.rend(); ++it) {
        recursions.erase(recursions.begin() + *it);
    }
}


bool Recursion::instantiateAFreeVariableOf(const ORightHandSide &rule) {
    ExprSymbolSet freeVars = std::move(getNonChainingFreeVariablesOf(rule));

    if (freeVars.empty()) {
        return false;
    }

    int num = 0;
    ExprSymbol freeVar = *(freeVars.begin());
    TT::Expression freeVarEx = TT::Expression(freeVar);
    for (const TT::Expression &ex : rule.guard) {
        bool con = false;
        for (const ExprSymbol &var : ex.getVariables()) {
            if (itrs.isChainingVariable(var)) {
                con = true;
            }
        }
        if (con) {
            continue;
        }

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
            sub.emplace(freeVar, rhs.toGiNaC());

            recursions.push_back(rule);
            recursions.back().substitute(sub);
            num++;

        } else if (rhs.equals(freeVarEx) && lhs.hasNoFunctionSymbols()) {
            GiNaC::exmap sub;
            sub.emplace(freeVar, lhs.toGiNaC());

            recursions.push_back(rule);
            recursions.back().substitute(sub);
            num++;
        }
    }

    if (num == 0) {
        GiNaC::exmap sub;
        sub.emplace(freeVar, GiNaC::numeric(1)); // default value

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
        ORightHandSide copy = recursions[i];
        if (evaluateASpecificRecursiveCallOf(copy)) {
            evaluated.push_back(i);
        }
    }

    for (auto it = evaluated.rbegin(); it != evaluated.rend(); ++it) {
        recursions.erase(recursions.begin() + *it);
    }
}


bool Recursion::evaluateASpecificRecursiveCallOf(const ORightHandSide &recursion) {
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
                if (guardI.info(TT::InfoFlag::RelationEqual)) {
                    TT::Expression lhs = guardI.op(0);
                    TT::Expression rhs = guardI.op(1);
                    if (lhs.info(TT::InfoFlag::Variable)
                        && itrs.isChainingVariable(GiNaC::ex_to<ExprSymbol>(lhs.toGiNaC()))
                        && !rhs.hasFunctionSymbol()) {
                        GiNaC::exmap guardSub;
                        guardSub.emplace(GiNaC::ex_to<ExprSymbol>(lhs.toGiNaC()),
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
    std::ostringstream debugOut;
    debugOut << "Real Variables: ";

    std::vector<TT::Expression> funApps;
    assert(!rhs.term.hasFunctionSymbol());
    for (const TT::Expression &ex : rhs.guard) {
        ex.collectFunctionApplications(funApps);
    }

    for (int i = 0; i < vars.size(); ++i) {
        ExprSymbol var = itrs.getGinacSymbol(vars[i]);
        debugRecursionDetailed("variable: " << var);

        for (const TT::Expression &funApp : funApps) {
            debugRecursionDetailed("function application: " << funApp);
            assert(funApp.nops() == vars.size());

            TT::Expression update = funApp.op(i);
            debugRecursionDetailed("update: " << update);
            assert(update.hasNoFunctionSymbols());

            if (var != update.toGiNaC()) {
                debugRecursionDetailed("real");
                realVars.insert(i);
                debugOut << var << ", ";
            }
        }
    }

    debugRecursion(debugOut.str());
    return realVars;
}


BaseCaseIndexMap Recursion::analyzeBaseCases(ExprSymbol mainVar) {
    debugRecursion("=== analyzing base cases ===");
    debugRecursionDetailed("mainVar: " << mainVar);
    BaseCaseIndexMap map;

    for (int i = 0; i < baseCases.size(); ++i) {
        const RightHandSide &bc = baseCases[i];
        std::vector<Expression> query;
        for (const TT::Expression &ex : bc.guard) {
            assert(ex.hasNoFunctionSymbols());
            query.push_back(ex.toGiNaC());
        }

        Z3VariableContext context;
        z3::model model(context, Z3_model());
        z3::check_result result;
        result = Z3Toolbox::checkExpressionsSAT(query, context, &model);

        debugRecursionDetailed("analyzing " << bc << " as a potential base case");
        if (result == z3::sat) {
            Expression value = Z3Toolbox::getRealFromModel(model, Expression::ginacToZ3(mainVar, context));
            if (value.info(GiNaC::info_flags::integer)
                && value.info(GiNaC::info_flags::nonnegative)) {
                // Check range?
                Purrs::index_type asUInt = GiNaC::ex_to<GiNaC::numeric>(value).to_int();
                if (map.count(asUInt) == 0) {
                    // Try to derive more base cases from one rhs?
                    debugRecursionDetailed("is a potential base case for " << mainVar << " = " << asUInt);
                    map.emplace(asUInt, i);
                    continue;

                } else {
                    debugRecursionDetailed("Discarding potential base case for " << mainVar << " = " << asUInt);
                }

            } else {
                debugRecursionDetailed("Error, " << value << " is not a natural number");
            }

        } else {
            debugRecursionDetailed("Z3 was not sat");
        }
    }

    for (const auto &pair : map) {
        debugRecursion(pair.first << ": " << pair.second);
    }

    return map;
}


bool Recursion::baseCasesMatch(const BaseCaseRhsMap &bcs,
                               const UpdateVector &updates,
                               const TT::ExpressionVector &normalGuard) {
    debugRecursion("=== Checking if base cases match ===");
    debugRecursion("Base Cases:");
    for (const auto &pair : bcs) {
        debugRecursion(pair.first << ": " << pair.second);
    }

    debugRecursion("Updates:");
    for (const GiNaC::exmap &update : updates) {
        debugRecursion(update);
    }

    debugRecursion("Guard:");
    for (const TT::Expression &ex : normalGuard) {
        debugRecursion(ex);
    }


    // check if the base cases match
    for (const GiNaC::exmap &update : updates) {
        debugRecursionDetailed("Now handling update: " << update);

        std::vector<std::vector<Expression>> queryRhs;
        // disjunction of conjunctions (updated base cases)
        debugRecursionDetailed("RHS:");
        for (auto const &pair : bcs) {
            debugRecursionDetailed("OR (updated base case guard)");
            std::vector<Expression> updatedGuard;
            for (const TT::Expression &ex : pair.second.guard) {
                assert(!ex.hasFunctionSymbol());

                updatedGuard.push_back(ex.toGiNaC().subs(update));
                debugRecursionDetailed("\tAND " << updatedGuard.back());
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
            debugRecursionDetailed("negating: " << negateEx);
            Expression toNegate = negateEx.toGiNaC().subs(update);
            queryLhs.push_back(GT::negate(toNegate));

            debugRecursionDetailed("LHS:");
            for (const Expression &ex : queryLhs) {
                debugRecursionDetailed("AND " << ex);
            }

            if (!Z3T::checkTautologicImplication(queryLhs, queryRhs)) {
                debugRecursionDetailed("FALSE");
                return false;
            }

            debugRecursionDetailed("TRUE");

            queryLhs.pop_back();
        }
    }

    return true;
}


VariableConfig Recursion::generateMultiVarConfig(const UpdateVector &updates, const std::set<int> &realVars, int mainVarIndex) {
    debugRecursion("=== Generating multivar config ===");
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
            debugRecursion("arg " << mainVarGiNaC << " -> " << varGiNaC << ": nether constant difference nor constant sum");
            config.error = true;
        }
    }

    return config;
}


bool Recursion::updatesHaveConstDifference(const UpdateVector &updates, int mainVarIndex, int varIndex) const {
    debugRecursionDetailed("=== Checking if updates have constant difference ===");
    debugRecursionDetailed("main var index: " << mainVarIndex);
    debugRecursionDetailed("checking var index: " << varIndex);

    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);
    VariableIndex var = funSymbol.getArguments()[varIndex];
    ExprSymbol varGiNaC = itrs.getGinacSymbol(var);

    for (const GiNaC::exmap &update : updates) {
        assert(update.count(mainVarGiNaC) > 0);
        assert(update.count(varGiNaC) > 0);

        TT::Expression updateMainVar = std::move(update.at(mainVarGiNaC));
        TT::Expression updateVar = std::move(update.at(varGiNaC));
        debugRecursionDetailed("Update (main var): " << updateMainVar << ", update: " << updateVar);
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


bool Recursion::updatesHaveConstSum(const UpdateVector &updates, int mainVarIndex, int varIndex) const {
    debugRecursionDetailed("=== Checking if updates have constant difference ===");
    debugRecursionDetailed("main var index: " << mainVarIndex);
    debugRecursionDetailed("checking var index: " << varIndex);

    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);
    VariableIndex var = funSymbol.getArguments()[varIndex];
    ExprSymbol varGiNaC = itrs.getGinacSymbol(var);

    for (const GiNaC::exmap &update : updates) {
        assert(update.count(mainVarGiNaC) > 0);
        assert(update.count(varGiNaC) > 0);

        TT::Expression updateMainVar = std::move(update.at(mainVarGiNaC));
        TT::Expression updateVar = std::move(update.at(varGiNaC));
        debugRecursionDetailed("Update (main var): " << updateMainVar << ", update: " << updateVar);
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


bool Recursion::selfReferentialGuardIsInductivelyValid(const TT::ExpressionVector &normalGuard,
                                                       const RecursiveCallMap &recCallMap,
                                                       const BaseCaseRhsMap &baseCaseMap,
                                                       const TT::ExpressionVector &srGuard) const {
    debugRecursion("=== Checking if the self-referential guard is inductively valid ===");
    TT::Expression dummy(GiNaC::numeric(0));
    TT::ExpressionVector dummyVector;

    // IB: For every base case: selfReferentialGuard[funSymbol/baseCase] holds
    for (auto const &pair : baseCaseMap) {
        TT::FunctionDefinition funDef(itrs, funSymbolIndex, pair.second.term, dummy, dummyVector);
        GiNaC::exmap evaluatedRecursiveCalls;
        for (const auto &pair : recCallMap) {
            TT::Expression eval = pair.second.evaluateFunction(funDef, nullptr, nullptr);
            assert(!eval.hasFunctionSymbol());
            evaluatedRecursiveCalls.emplace(pair.first, eval.toGiNaC());
        }

        std::vector<Expression> evaluatedSRGuard;
        for (const TT::Expression &ex : srGuard) {
            assert(!ex.hasFunctionSymbol());
            evaluatedSRGuard.push_back(ex.toGiNaC().subs(evaluatedRecursiveCalls));
        }

        std::vector<Expression> query;
        for (const Expression &ex : evaluatedSRGuard) {
            if (!Z3Toolbox::checkTautology(ex)) {
                debugRecursionDetailed("query: " << ex << ": FALSE");
                return false;

            }
            debugRecursionDetailed("query: " << ex << ": TRUE");
        }
    }

    // IS: For the recursion: guard && srGuard => recursion.term
    /*TT::FunToVarSub sub; // make sure that identical function calls are substituted by the same variable
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
    }*/

    return true;
}


bool Recursion::computeClosedFormOfTheRHSs(TT::Expression &closed,
                                           const RightHandSide &recursion,
                                           int mainVarIndex,
                                           const VariableConfig &config,
                                           const RecursiveCallMap &recCallMap,
                                           const BaseCaseRhsMap &baseCaseMap) {
    debugRecursion("=== Computing closed form of the right-hand sides ===");
    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);

    // Insert recursive calls
    TT::Expression rhs = recursion.term.unGinacify().substitute(recCallMap);

    // Substitute non-main variables
    rhs = rhs.substitute(config.sub);

    // Insert purrs n and turn into purrs recurrence
    GiNaC::exmap purrsSub;
    purrsSub.emplace(mainVarGiNaC, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
    Purrs::Expr recurrence = rhs.substitute(purrsSub).toPurrs(mainVarIndex);

    PurrsBaseCases baseCasesPurrs;
    for (auto const &pair : baseCaseMap) {
        // Substitute non-main variables
        TT::Expression res = pair.second.term.substitute(config.sub);
        baseCasesPurrs.emplace(pair.first, res.toPurrs());
    }

    if (!solve(recurrence, baseCasesPurrs)) {
        debugRecursion("Could not solve recurrence");
        return false;
    }

    // re-substitute n
    GiNaC::exmap purrsReSub;
    purrsReSub.emplace(Purrs::Expr(Purrs::Recurrence::n).toGiNaC(), mainVarGiNaC);
    closed = TT::Expression(recurrence.toGiNaC().subs(purrsReSub));

    // re-substitute non-main variables
    closed = closed.substitute(config.reSub);

    return true;
}


bool Recursion::computeClosedFormOfTheCosts(TT::Expression &closed,
                                            const TT::Expression &closedRHSs,
                                            const RightHandSide &recursion,
                                            int mainVarIndex,
                                            const VariableConfig &config,
                                            const RecursiveCallMap &recCallMap,
                                            const std::vector<TT::Expression> &recCalls,
                                            const BaseCaseRhsMap &baseCaseMap) {
    debugRecursion("=== Computing closed form of the costs ===");
    VariableIndex mainVar = funSymbol.getArguments()[mainVarIndex];
    ExprSymbol mainVarGiNaC = itrs.getGinacSymbol(mainVar);
    TT::Expression dummy(GiNaC::numeric(0));
    TT::ExpressionVector dummyVector;

    // evaluate recursive calls using closed form
    TT::FunctionDefinition funDef(itrs, funSymbolIndex, closedRHSs, dummy, dummyVector);
    GiNaC::exmap evaluatedRecursiveCalls;
    for (const auto &pair : recCallMap) {
        TT::Expression eval = pair.second.evaluateFunction(funDef, nullptr, nullptr);
        assert(!eval.hasFunctionSymbol());
        evaluatedRecursiveCalls.emplace(pair.first, eval.toGiNaC());
    }

    // insert them into the cost
    TT::Expression costRecurrence = recursion.cost.substitute(evaluatedRecursiveCalls);

    // add the recursive calls
    for (TT::Expression recCall : recCalls) {
        costRecurrence += recCall;
    }

    // Substitute non-main variables
    costRecurrence = costRecurrence.substitute(config.sub);

    // Insert purrs n and turn into purrs recurrence
    GiNaC::exmap purrsSub;
    purrsSub.emplace(mainVarGiNaC, Purrs::Expr(Purrs::Recurrence::n).toGiNaC());
    Purrs::Expr recurrence = costRecurrence.substitute(purrsSub).toPurrs(mainVarIndex);

    PurrsBaseCases baseCasesPurrs;
    for (auto const &pair : baseCaseMap) {
        // Substitute non-main variables
        TT::Expression res = pair.second.cost.substitute(config.sub);
        baseCasesPurrs.emplace(pair.first, res.toPurrs());
    }

    if (!solve(recurrence, baseCasesPurrs)) {
        debugRecursion("Could not solve cost recurrence");
        return false;
    }

    // re-substitute n
    GiNaC::exmap purrsReSub;
    purrsReSub.emplace(Purrs::Expr(Purrs::Recurrence::n).toGiNaC(), mainVarGiNaC);
    closed = TT::Expression(recurrence.toGiNaC().subs(purrsReSub));

    // re-substitute non-main variables
    closed = closed.substitute(config.reSub);

    return true;
}


bool Recursion::solve(Purrs::Expr &recurrence, const PurrsBaseCases &bc) {
    debugRecursion("Solving recurrence: " << recurrence);
    Purrs::Expr exact;

    try {
        Purrs::Recurrence rec(recurrence);
        debugRecursion("base cases:");
        for (auto const &pair : bc) {
            debugRecursion("n = " << pair.first << " is " << pair.second);
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

TT::ExpressionVector Recursion::constructGuard(const TT::ExpressionVector &normalGuard,
                                               const BaseCaseRhsMap &baseCaseMap) const {
    debugRecursion("=== Constructing guard ===");
    TT::ExpressionVector guard(normalGuard);

    for (auto const &pair : baseCaseMap) {
        TT::ExpressionVector cleanedGuard;
        for (const TT::Expression &ex : pair.second.guard) {
            TT::Expression lhs = ex.op(0);
            TT::Expression rhs = ex.op(1);
            Expression minus = lhs.toGiNaC() - rhs.toGiNaC();

            if (!(ex.info(TT::InfoFlag::RelationEqual)
                  || ex.info(TT::InfoFlag::RelationGreaterEqual)
                  || ex.info(TT::InfoFlag::RelationLessEqual))
                || !(minus.is_zero())) {
                cleanedGuard.push_back(ex);
            }
        }

        if (cleanedGuard.size() == 1) {
            TT::Expression bcGuard = cleanedGuard[0];

            if (bcGuard.info(TT::InfoFlag::RelationEqual)) {
                TT::Expression bcLhs = bcGuard.op(0).ginacify();
                TT::Expression bcRhs = bcGuard.op(1).ginacify();

                for (TT::Expression &ex : guard) {
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

    for (TT::Expression &ex : guard) {
        ex = ex.ginacify();
    }

    return guard;
}


ExprSymbolSet Recursion::getNonChainingFreeVariablesOf(const RightHandSide &rule) const {
    ExprSymbolSet freeVars;
    rule.term.collectVariables(freeVars);
    rule.cost.collectVariables(freeVars);
    for (const TT::Expression &ex : rule.guard) {
        ex.collectVariables(freeVars);
    }

    // delete all non-free variables and all chaining variables
    auto it = freeVars.begin();
    while (it != freeVars.end()) {
        if (itrs.isFreeVariable(*it) && !(itrs.isChainingVariable(*it))) {
            ++it;

        } else {
            it = freeVars.erase(it);
        }
    }

    return freeVars;
}


ExprSymbolSet Recursion::getChainingVariablesOf(const TT::Expression &ex) const {
    ExprSymbolSet chainVars;
    ex.collectVariables(chainVars);

    // delete all non-chaining variables
    auto it = chainVars.begin();
    while (it != chainVars.end()) {
        if (itrs.isChainingVariable(*it)) {
            ++it;

        } else {
            it = chainVars.erase(it);
        }
    }

    return chainVars;
}

ExprSymbolSet Recursion::getChainingVariablesOf(const RightHandSide &rule) const {
    ExprSymbolSet chainVars;
    rule.term.collectVariables(chainVars);
    rule.cost.collectVariables(chainVars);
    for (const TT::Expression &ex : rule.guard) {
        ex.collectVariables(chainVars);
    }

    // delete all non-chaining variables
    auto it = chainVars.begin();
    while (it != chainVars.end()) {
        if (itrs.isChainingVariable(*it)) {
            ++it;

        } else {
            it = chainVars.erase(it);
        }
    }

    return chainVars;
}


void Recursion::dumpRightHandSides() const {
    debugRecursion("**********");
    debugRecursion("Recursions:");
    for (const RightHandSide &rhs: recursions) {
        debugRecursion(rhs);
    }
    debugRecursion("Base Cases:");
    for (const RightHandSide &rhs: baseCases) {
        debugRecursion(rhs);
    }
    debugRecursion("**********");
}