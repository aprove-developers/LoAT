# LoAT

LoAT (**L**ower bounds **A**nalysis **T**ool) is a tool to automatically infer lower bounds on the worst-case runtime complexity of integer programs (i.e. integer transition systems).

LoAT has been inspired by [KoAT](<https://github.com/s-falke/kittel-koat/>) and uses a variation of ranking functions in combination with recurrence solving to prove lower bounds.
The tool is based on the recurrence solver [PURRS](http://www.cs.unipr.it/purrs/) and the SMT solver [Z3](https://github.com/Z3Prover/z3/).


## Paper

The technique behind LoAT is described in [this](http://aprove.informatik.rwth-aachen.de/eval/integerLower/compl-paper.pdf) paper.


## Website

More information, a detailed evaluation and a static binary can be found on our [website](http://aprove.informatik.rwth-aachen.de/eval/integerLower/).
