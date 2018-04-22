#ifndef SRC_BACKWARDACCELERATION_H_
#define SRC_BACKWARDACCELERATION_H_

#include "flowgraph.h"
#include <boost/optional.hpp>

class BackwardAcceleration {
public:
    BackwardAcceleration(ITRSProblem &itrs, Transition t);
    boost::optional<Transition> accelerate();

private:
    bool costFunctionSupported();
    std::vector<Expression> reduceGuard(Z3VariableContext &c);
    boost::optional<std::vector<VariableIndex>> dependencyOrder(UpdateMap &update);
    boost::optional<std::pair<GiNaC::exmap, UpdateMap>> computeInverseUpdate();
    bool checkGuardImplication(GiNaC::exmap inverse_update);
    boost::optional<GiNaC::exmap> computeIteratedInverseUpdate(GiNaC::exmap inverse_update, std::vector<VariableIndex> order);
    boost::optional<Expression> computeIteratedCosts(GiNaC::exmap iterated_inverse_update);
    Transition buildNewTransition(GiNaC::exmap inverse_update, GiNaC::exmap iterated_inverse_update, Expression iterated_costs);
    bool mapsToInt(Expression e);

private:
    Transition trans;
    ITRSProblem &itrs;
    Expression ginac_n;
};

#endif /* SRC_BACKWARDACCELERATION_H_ */
