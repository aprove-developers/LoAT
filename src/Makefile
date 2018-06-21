OBJECTS = \
  config.o \
  global.o \
  util/timing.o \
  util/stats.o \
  util/timeout.o \
  accelerate/accelerator.o \
  accelerate/backward.o \
  accelerate/meter/farkas.o \
  accelerate/meter/metertools.o \
  accelerate/meter/linearize.o \
  accelerate/meter/metering.o \
  accelerate/recurrence/dependencyorder.o \
  accelerate/recurrence/recurrence.o \
  accelerate/forward.o \
  its/export.o \
  its/types.o \
  its/itsproblem.o \
  its/parser/termparser.o \
  its/parser/term.o \
  its/parser/itsparser.o \
  its/rule.o \
  its/variablemanager.o \
  asymptotic/limitvector.o \
  asymptotic/limitproblem.o \
  asymptotic/limitsmt.o \
  asymptotic/asymptoticbound.o \
  asymptotic/inftyexpression.o \
  analysis/prune.o \
  analysis/preprocess.o \
  analysis/chain.o \
  analysis/analysis.o \
  analysis/chainstrategy.o \
  expr/relation.o \
  expr/ginactoz3.o \
  expr/expression.o \
  expr/guardtoolbox.o \
  expr/complexity.o \
  z3/z3context.o \
  z3/z3toolbox.o


.PHONY: clean all

loat: main.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

koatToT2: converter/koatToT2.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

koatToComplexity: converter/koatToComplexity.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

clean:
	rm -f loat main.o $(OBJECTS)
