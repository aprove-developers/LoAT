# Installation instructions

## Prerequisites

To compile LoAT, you will need the following libraries (and all their dependencies):

 * GiNaC (<http://www.ginac.de>)
 * a slightly patched version of PURRS (<https://github.com/mttnff/LoAT-purrs>)
 * Z3 (<https://github.com/Z3Prover/z3>)

(the dependencies include: CLN, NTL, giac, gf2x, cgicc)


## Static Binary

If you just want to try out LoAT, we also offer a precompiled static binary on our website
(<http://aprove.informatik.rwth-aachen.de/eval/integerLower>) for convenience.


## Compilation

Please note that LoAT requires a C++11 compliant compiler (e.g. recent g++/clang++).
There isn't really a build system for now, so just type:

```
  $ make
```

which should compile LoAT. You might need to add some linker/include flags if you
installed the libraries in non standard directories (as documented in the `Makefile`).

LoAT can then be run with

```
  $ ./loat
```

You might want to run LoAT on the included examples (see `example` directory) to see if
it works as expected. There are some options to play around with in `src/global.h`.


## Useful Scripts

If you use the dot output (see `./loat --help`), you might want to use `dot2pdf.sh` (or
`splitdot2pdf.sh`) which will convert the dot file to a single (or multiple) pdf files
(this requires that the graphviz package containing dot is installed).

There is also a small benchmark script (`benchscore.sh`), which is probably less interesting.
