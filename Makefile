# Aadditional compiler flags (add include flags for purrs/ginac/z3 etc. if neccessary).
COMPILE_FLAGS = -O2 -g

# Additioanl linker flags (add corresponding library paths here)
LINK_FLAGS =

# All flags used for compiling/linking
CXXFLAGS = -std=c++11 $(COMPILE_FLAGS)
LDFLAGS = -lpurrs -lginac -lcln -lntl -lz3 $(LINK_FLAGS)

# Alternative flags used to produce a static binary
CXXFLAGS_STATIC = $(CXXFLAGS) -static
LDFLAGS_STATIC = -lpurrs -lginac -lcln -lntl -lz3 -lgmp -lgomp -lpthread $(LINK_FLAGS)

SRCDIR = src

.PHONY: clean cleanstatic all loat static koatToT2 koatToComplexity

all: loat

loat:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" loat
	ln -f -s $(SRCDIR)/loat loat

static:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS_STATIC)" LDFLAGS="$(LDFLAGS_STATIC)" loat
	cp $(SRCDIR)/loat loat-static && strip loat-static

koatToT2:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" koatToT2
	ln -f -s $(SRCDIR)/koatToT2 koatToT2

koatToComplexity:
	$(MAKE) -C $(SRCDIR) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" koatToComplexity
	ln -f -s $(SRCDIR)/koatToComplexity koatToComplexity

clean:
	$(MAKE) -C $(SRCDIR) clean
	rm -f loat

cleanstatic: clean
	rm -f loat-static
