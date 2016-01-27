#additional compiler flags (add include flags for purrs/ginac/z3 etc. if neccessary)
#this is the default for debugging, override using e.g. make COMPILE_FLAGS=-O2
COMPILE_FLAGS = -O0 -g

#additioanl linker flags (add corresponding library paths here)
LINK_FLAGS =

#all flags used for compiling/linking
CXXFLAGS = -std=c++11 $(COMPILE_FLAGS)
CXXFLAGS_STATIC = $(CXXFLAGS) -static
LDFLAGS = -lpurrs -lginac -lcln -lntl -lz3 $(LINK_FLAGS)
LDFLAGS_STATIC = -lpurrs -lginac -lcln -lntl -lz3 -lgmp -lgomp -lpthread $(LINK_FLAGS)

SRCDIR = src

.PHONY: clean cleanstatic all

all: loat

loat:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" loat
	ln -f -s $(SRCDIR)/loat loat

static:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS_STATIC)" LDFLAGS="$(LDFLAGS_STATIC)" loat
	cp $(SRCDIR)/loat loat-static && strip loat-static

koatToT2:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" koatToT2

koatToComplexity:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" koatToComplexity

clean:
	$(MAKE) -C $(SRCDIR) clean
	rm -f loat

cleanstatic: clean
	rm -f loat-static
