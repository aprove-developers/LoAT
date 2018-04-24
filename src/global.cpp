#include "global.h"

bool GlobalFlags::limitSmt = false;

#ifndef PROOF_OUTPUT_ENABLE
    std::ostream DummyStream::dummy(0);
    ProofOutput GlobalVariables::proofOutput(DummyStream::dummy, false);
#else
    #ifdef PROOF_OUTPUT_COLORS
        ProofOutput GlobalVariables::proofOutput(std::cout, true);
    #else
        ProofOutput GlobalVariables::proofOutput(std::cout, false);
    #endif
#endif



