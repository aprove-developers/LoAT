#ifndef VARIABLEMANAGER_H
#define VARIABLEMANAGER_H

#include "rule.h"


// Abbreviation since the VariableManager is passed around quite a bit
class VariableManager;
typedef VariableManager VarMan;


/**
 * Manages variables, i.e., can map between variable indices, names and GiNaC symbols.
 * Also manages the set of temporary/free variables.
 *
 * This class is used as a part of an ITSProblem, but is separate since many functions
 * only need variable management, not the full ITSProblem.
 */
class VariableManager {
public:
    // Mapping between indices and names
    bool hasVarIdx(VariableIdx idx) const;
    std::string getVarName(VariableIdx idx) const;
    VariableIdx getVarIdx(std::string name) const;

    // Mapping between indices and ginac symbols
    VariableIdx getVarIdx(const ExprSymbol &var) const; // TODO: Use this instead of getVarIdx(symbol.get_name())
    ExprSymbol getGinacSymbol(VariableIdx idx) const; // TODO: Shorten this to getSymbol()?
    ExprList getGinacVarList() const; // TODO: Rename to getSymbolList() ?

    // Handling of temporary variables
    const std::set<VariableIdx>& getTempVars() const;
    bool isTempVar(VariableIdx idx) const;
    bool isTempVar(const ExprSymbol &var) const;

    /**
     * Adds a new fresh variable based on the given name
     * (the given name is used if it is still available, otherwise it is modified)
     * @return the VariableIdx of the newly added variable
     */
    VariableIdx addFreshVariable(std::string basename);
    VariableIdx addFreshTemporaryVariable(std::string basename);

    /**
     * Generates a fresh (unused) GiNaC symbol, but does _not_ add it to the list of variables
     *
     * @warning The name of the created symbol is not stored, so it may be re-used by future calls!
     * Note that two generated symbols are always different (to GiNac), even if they use the same name.
     *
     * @return The newly created symbol (_not_ associated with a variable index!)
     */
    ExprSymbol getFreshUntrackedSymbol(std::string basename) const;

protected:
    size_t getVariableCount() const;

private:
    // Adds a variable with the given name to all relevant maps, returns the new index
    VariableIdx addVariable(std::string name);

    // Generates a yet unused name starting with the given string
    std::string getFreshName(std::string basename) const;

private:
    // Data stored for each variable
    struct Variable {
        std::string name;
        ExprSymbol symbol;
    };

    // List of all variables (VariableIdx is an index in this list; a Variable is a name and a ginac symbol)
    // Note: Variables are never removed, so this list is appended, but otherwise not modified
    std::vector<Variable> variables;

    // The set of variables (identified by their index) that are used as temporary variables (not bound by lhs)
    std::set<VariableIdx> temporaryVariables;

    // Reverse mapping for efficiency
    std::map<std::string,VariableIdx> variableNameLookup;

    // List of all variable symbols. Useful as argument to GiNaC::ex::is_polynomial.
    ExprList variableSymbolList;
};





#endif //VARIABLEMANAGER_H
