OBJECTS = its.o expression.o flowgraph.o recurrence.o z3toolbox.o farkas.o stats.o preprocess.o infinity.o asymptotic/inftyexpression.o asymptotic/limitvector.o asymptotic/limitproblem.o asymptotic/asymptoticbound.o guardtoolbox.o timing.o debug.o timeout.o

.PHONY: clean all

loat: main.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

koatToT2: koatToT2.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

koatToComplexity: koatToComplexity.o expression.o z3toolbox.o timing.o timeout.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

clean:
	rm -f loat main.o $(OBJECTS)
