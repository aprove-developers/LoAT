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

    for (const RightHandSide *rhs : recursions) {
        defTerm = recursion->term;
        TT::Substitution sub;
        std::vector<int> ignoreInGuard;
        for (int i = 0; i < rhs->guard.size(); ++i) {
            const TT::Expression &ex = rhs->guard[i];

            if (ex.info(TT::InfoFlag::RelationEqual)) {
                TT::Expression lhs = ex.op(0);
                if (lhs.info(TT::InfoFlag::Variable)) {
                    ExprSymbol var = GiNaC::ex_to<GiNaC::symbol>(lhs.toGiNaC());
                    if (itrs.isFreeVariable(var)) {
                        if (sub.count(var) == 0) {
                            sub.emplace(var, ex.op(1));
                            defTerm = defTerm.unGinacify().substitute(sub);
                            ignoreInGuard.push_back(i);
                        }
                    }
                }
            }
        }

        recursion = rhs;
        std::set<int> realVars = std::move(findRealVars(defTerm));

        if (realVars.empty()) {
            debugRecursion("Recursion has no real vars!");

        } else if (realVars.size() == 1) {
            realVarIndex = *realVars.begin();
            realVar = funSymbol.getArguments()[realVarIndex];
            realVarGiNaC = itrs.getGinacSymbol(realVar);

            if (!findBaseCases()) {
                debugRecursion("Found no usable base cases");
                return false;
            }

            recursionCopy = *rhs;
            for (int i = ignoreInGuard.size() - 1; i >= 0; i--) {
                recursionCopy.guard.erase(recursionCopy.guard.begin() + ignoreInGuard[i]);
            }
            for (TT::Expression &ex : recursionCopy.guard) {
                ex = ex.unGinacify().substitute(sub);
            }
            recursionCopy.cost = recursionCopy.cost.unGinacify().substitute(sub);

            if (solveRecursionInOneVar()) {
                wereUsed.insert(rhs);
            }

        } else if (realVars.size() == 2) {
            realVarIndex = *realVars.begin();
            realVar = funSymbol.getArguments()[realVarIndex];
            realVarGiNaC = itrs.getGinacSymbol(realVar);

            realVar2Index = *(++realVars.begin());
            realVar2 = funSymbol.getArguments()[realVar2Index];
            realVar2GiNaC = itrs.getGinacSymbol(realVar2);

            // solveRecursionInTwoVars makes the copy
            if (solveRecursionInTwoVars()) {
                wereUsed.insert(rhs);
            }

            // Second try: Swap the variables
            std::swap(realVarIndex, realVar2Index);
            std::swap(realVar, realVar2);
            std::swap(realVarGiNaC, realVar2GiNaC);

            // solveRecursionInTwoVars makes the copy
            if (solveRecursionInTwoVars()) {
                wereUsed.insert(rhs);
            }

        } else {
            debugRecursion("recursion contains too many real vars");
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

        substituteFreeVariables();

        // Remove self-referential conditions from the guard
        TT::ExpressionVector selfReferentialGuard;
        TT::ExpressionVector &rcGuard = recursionCopy.guard;
        auto it = rcGuard.begin();
        while (it != rcGuard.end()) {
            if (it->hasFunctionSymbol(funSymbolIndex)) {
                selfReferentialGuard.push_back(std::move(*it));
                it = rcGuard.erase(it);

            } else {
                ++it;
            }
        }

        // try to instantiate variables if the base cases are not sufficient
        instCandidates.clear();
        for (TT::Expression &ex : rcGuard) {
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

        evaluateConstantRecursiveCalls();


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

void Recursion::substituteFreeVariables() {
    debugRecursion("===Substituting free variables===");
    ExprSymbolSet freeVars;

    recursionCopy.term.collectVariables(freeVars);
    recursionCopy.cost.collectVariables(freeVars);
    for (const TT::Expression &ex : recursionCopy.guard) {
        ex.collectVariables(freeVars);
    }

    auto it = freeVars.begin();
    while (it != freeVars.end()) {
        if (itrs.isFreeVariable(*it)) {
            ++it;

        } else {
            it = freeVars.erase(it);
        }
    }

    freeVarSub.clear();
    for (const ExprSymbol &symbol : freeVars) {
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
            TT::Expression symbolEx = TT::Expression(symbol);

            if (lhs.equals(symbolEx) && rhs.hasNoFunctionSymbols()) {
                freeVarSub.emplace(symbol, rhs.toGiNaC());
                break;

            } else if (rhs.equals(symbolEx) && lhs.hasNoFunctionSymbols()) {
                freeVarSub.emplace(symbol, lhs.toGiNaC());
                break;
            }
        }

        if (freeVarSub.count(symbol) == 0) {
            freeVarSub.emplace(symbol, GiNaC::numeric(1));
        }
    }

    for (auto const &pair : freeVarSub) {
        debugRecursion(pair.first << " -> " << pair.second);
    }

    recursionCopy.substitute(freeVarSub);
}


void Recursion::evaluateConstantRecursiveCalls() {
    debugRecursion("===Trying to evaluate constant recursive calls");

    for (auto const &pair : baseCases) {
        const RightHandSide &rhs = *pair.second;

        TT::FunctionDefinition funDef(itrs, funSymbolIndex, rhs.term, rhs.cost, rhs.guard);

        debugRecursion("Before: " << recursionCopy.term);
        recursionCopy.term = recursionCopy.term.evaluateFunctionIfLegal(funDef, recursionCopy.guard, &recursionCopy.cost);
        debugRecursion("After: " << recursionCopy.term);
    }
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

    recursionCopy.substitute(instSub);
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


bool Recursion::updatesHaveConstSum(const TT::Expression &term) const {
    debugRecursion("===Checking if updates have constant sum===");
    std::vector<TT::Expression> funApps = std::move(term.getFunctionApplications());

    for (const TT::Expression &funApp : funApps) {
        TT::Expression update = std::move(funApp.op(realVarIndex));
        TT::Expression update2 = std::move(funApp.op(realVar2Index));
        debugRecursion("Update: " << update << ", update 2: " << update2);
        assert(update.hasNoFunctionSymbols());
        assert(update2.hasNoFunctionSymbols());
        GiNaC::ex updateGiNaC = update.toGiNaC();
        GiNaC::ex update2GiNaC = update2.toGiNaC();

        // e.g. (x - 1) + (y + 1) = x + y
        GiNaC::ex sum = updateGiNaC + update2GiNaC;
        // e.g. x + y
        GiNaC::ex varSum = realVarGiNaC + realVar2GiNaC;

        if (!sum.is_equal(varSum)) {
            return false;
        }

        // x - 1 - ((y + 1)[y/x]) is a number
        GiNaC::exmap sub;
        sub.emplace(realVar2GiNaC, realVarGiNaC);
        GiNaC::ex ex = updateGiNaC - update2GiNaC.subs(sub);
        if (!GiNaC::is_a<GiNaC::numeric>(ex)) {
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

            // Make sure that there are no nested function symbols
            std::vector<TT::Expression> updates = std::move(rhs->term.getUpdates());
            for (const TT::Expression ex : updates) {
                if (ex.hasFunctionSymbol()) {
                    debugRecursion("recursion has nested function symbol");
                    continue;
                }
            }

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

            debugRecursion("Recursion is suitable");
            recursions.push_back(rhs);
        }
    }

    for (const RightHandSide *rhs : recursions) {
        rightHandSides.erase(rhs);
    }

    return !recursions.empty();
}


std::set<int> Recursion::findRealVars(const TT::Expression &term) {
    debugRecursion("===Finding real recursion variables===");
    std::set<int> realVars;
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
            assert(update.hasNoFunctionSymbols());

            if (var != update.toGiNaC()) {
                debugRecursion("real");
                realVars.insert(i);
            }
        }
    }

    return realVars;
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
        TT::Expression update = funApp.op(realVarIndex).substitute(freeVarSub).substitute(instSub);
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